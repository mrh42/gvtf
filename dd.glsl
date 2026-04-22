struct DoubleDouble {
    double hi;
    double lo;
};

// Helper: Add a standard double to a Double-Double
DoubleDouble dd_add_d(DoubleDouble a, double b) {
    precise double S = a.hi + b;
    precise double v = S - a.hi;
    precise double err = (a.hi - (S - v)) + (b - v);
    return DoubleDouble(S, err + a.lo);
}


// Stripped down, branchless conversion assuming max 160-bit X
DoubleDouble toDD_192(uint192 A) {
    DoubleDouble res = {0.0, 0.0};
    
    // Unconditionally process only the limbs that can actually 
    // affect the top 106 bits. No 'if' statements to break the warp.
    res = dd_add_d(res, double(A.H.x[2]) * p160);
    res = dd_add_d(res, double(A.H.x[1]) * p128); // Bits 128-159
    res = dd_add_d(res, double(A.H.x[0]) * p96);  // Bits 96-127
    res = dd_add_d(res, double(A.L.x[2]) * p64);  // Bits 64-95
    res = dd_add_d(res, double(A.L.x[1]) * p32);
    res = dd_add_d(res, double(A.L.x[0]));
    
    // We ignore L.x[0], L.x[1], and H.x[2] completely.
    return res;
}

void fto96_dd(DoubleDouble val, out uint96 D) {
    // 1. Explicitly floor the high part
    double f_hi = floor(val.hi);
    double f_lo = 0.0;
    
    // 2. If 'hi' is an exact integer, any fractional data is in 'lo'.
    // We must floor 'lo' to ensure negative fractions pull the total down by 1.
    if (f_hi == val.hi) {
        f_lo = floor(val.lo);
    }
    
    // Now f_hi and f_lo are mathematically guaranteed to be exact integers.
    // We can safely use the truncating fto96 function.
    fto96(f_hi, D);
    
    if (f_lo < 0.0) {
        uint96 D_lo;
        fto96(-f_lo, D_lo);
        Sub(D, D_lo);
    } else if (f_lo > 0.0) {
        uint96 D_lo;
        fto96(f_lo, D_lo);
        Add(D, D_lo);
    }
}

//
// Assumes uint96 has limbs v[0], v[1], v[2] (least to most significant)
//

DoubleDouble toDD_96(uint96 val) {
    DoubleDouble res = {0.0, 0.0};
    

    // Add from most significant to least significant 
    // to preserve maximum precision in the hi component.
    if (val.x[2] != 0u) res = dd_add_d(res, double(val.x[2]) * p64);
    if (val.x[1] != 0u) res = dd_add_d(res, double(val.x[1]) * p32);
    if (val.x[0] != 0u) res = dd_add_d(res, double(val.x[0]));
    
    return res;
}


// ==========================================
// Double-Double Multiplication
// ==========================================
DoubleDouble dd_mul(DoubleDouble a, DoubleDouble b) {
    // 1. Exact multiplication of high parts
    precise double hi = a.hi * b.hi;
    
    // 2. Capture the exact error using native hardware FMA
    // fma(x, y, z) computes (x * y) + z with a single rounding step.
    precise double err = fma(a.hi, b.hi, -hi);
    
    // 3. Add the cross products (a.lo * b.lo is too small to matter at 106-bit)
    err += (a.hi * b.lo) + (a.lo * b.hi);
    
    // 4. QuickTwoSum normalization
    precise double sum_hi = hi + err;
    precise double sum_lo = err - (sum_hi - hi);
    
    return DoubleDouble(sum_hi, sum_lo);
}

//
// Double-Double Subtraction (Needed for Reciprocal)
//
DoubleDouble dd_sub(DoubleDouble a, DoubleDouble b) {
    precise double S = a.hi - b.hi;
    precise double v = S - a.hi;
    
    // Capture subtraction error
    precise double error_hi = (a.hi - (S - v)) - (b.hi + v);
    
    // Accumulate the lower parts
    double total_error = error_hi + a.lo - b.lo;
    
    // Normalize via QuickTwoSum
    precise double hi = S + total_error;
    precise double lo = total_error - (hi - S);
    
    return DoubleDouble(hi, lo);
}

//
// Double-Double Reciprocal ( 1.0 / Q )
//
DoubleDouble dd_reciprocal(DoubleDouble Q) {
    // 1. Generate the initial 53-bit guess using the hardware divider
    double x_approx = 1.0 / Q.hi;
    DoubleDouble x = DoubleDouble(x_approx, 0.0);
    
    DoubleDouble two = DoubleDouble(2.0, 0.0);
    
    // 2. Newton-Raphson iteration: x' = x * (2.0 - Q * x)
    // This doubles the precision from 53 bits to 106 bits.
    
    DoubleDouble Qx = dd_mul(Q, x);         // Compute Q * x
    DoubleDouble diff = dd_sub(two, Qx);    // Compute 2.0 - (Q * x)
    
    return dd_mul(x, diff);                 // Compute x * diff
}

bool SqMod(inout uint96 A, uint96 Q, bool doshift, DoubleDouble qinv_dd) {
    uint192 X, Y;
    uint96 D; // D2 and D3 removed as they appeared unused in the original snippet

    Mul192(A, A, X);

    if (doshift) {
        Lsh(X);
    }

    int i = 0;
    bool under = false;

    // The Iterative Barrett Reduction Loop
    while (!under && Gt(X, Q)) {
        // 1. Convert Current Remainder to 106-bit Float
        DoubleDouble X_dd = toDD_192(X);
        
        // 2. High-Precision Multiplication (Estimate Quotient)
        DoubleDouble D_dd = dd_mul(X_dd, qinv_dd);
        
        // 3. Convert EXACT Quotient back to 96-bit Integer
        fto96_dd(D_dd, D);

        // Fallback to guarantee forward progress if underflow in float conversion
        if (Zero(D)) {
            D.x[0] = 1; 
        }

        // 4. Multiply and Subtract
        Mul192(D, Q, Y);
        under = Sub(X, Y);
        i++;
    }

    // With Double-Double, i should almost never exceed 1 or 2.
    if (i > 2) {
	    atomicAdd(Debug[0], 1); 
    }

    if (!under) {
        A = X.L; 
    }

    return under;
}


bool tfdd(uint96 k) {
    uint96 sq, q, pp;
    uint192 t;

    // q = 2 * p * k + 1
    to96(P, pp);
    Mul192(k, pp, t);
    q = t.L;  // q is limited to 96-bits
    Lsh(q);
    Inc(q);

    int top = int(findMSB(P));
    uint64_t one = 1ul << top;

    // 1. Convert exactly to 106-bit DoubleDouble (no precision lost)
    DoubleDouble q_dd = toDD_96(q); 
    
    // 2. Compute exact reciprocal using Newton-Raphson
    DoubleDouble qinv_dd = dd_reciprocal(q_dd);
    
    // 3. Apply a microscopic underestimate to prevent overflow in SqMod
    // We use a value incredibly close to 1.0, e.g., 1.0 - 2^-100
    //DoubleDouble scale_down = {1.0, -7.88860905221e-31}; 
    DoubleDouble scale_down = {1.0, -1.0e-31lf}; 
    //DoubleDouble scale_down =     {0.999999999999lf, 0.0}; 
    qinv_dd = dd_mul(qinv_dd, scale_down);

    sq.x = uvec3(1, 0, 0);
    bool under;

    // Do the TF math
    for (int b = top; b >= 0; b--) {
        bool bb = (P & (one)) != 0;
        one >>= 1;
        
        // Pass the Double-Double qinv to SqMod
        under = SqMod(sq, q, bb, qinv_dd); 

        if (under) {
            atomicAdd(Debug[0], k.x[0]);
        }
    }

    return sq.x[0] == 1 && sq.x[1] == 0 && sq.x[2] == 0;
}

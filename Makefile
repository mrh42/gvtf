gvtf: spv32.h spv64.h tf.c tf.h main.go
	go build

spv32.h: tf.comp
	glslangValidator --target-env spirv1.6 --vn spv32 -V tf.comp -o spv32.h

spv64.h: tf64.comp
	glslangValidator --target-env spirv1.6 --vn spv64 -V tf64.comp -o spv64.h

gvtf: spv32.h spv192.h spv256.h tf.c tf.h gvtf.go go.mod
	go build

spv32.h: tf32.comp
	glslangValidator --target-env spirv1.6 --vn spv32 -V tf32.comp -o spv32.h

spv192.h: tf64.comp
	glslangValidator --target-env spirv1.6 -DSqModXX=SqMod9 --vn spv192 -V tf64.comp -o spv192.h

spv256.h: tf64.comp
	glslangValidator --target-env spirv1.6 -DSqModXX=SqMod --vn spv256 -V tf64.comp -o spv256.h

go.mod:
	go mod init gvtf
	go mod tidy

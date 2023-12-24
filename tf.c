#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "tf.h"

VkInstance instance;
VkDebugReportCallbackEXT debugReportCallback;
VkPhysicalDevice physicalDevice;
VkDevice device;
VkPipeline pipeline;
VkPipelineLayout pipelineLayout;
VkShaderModule computeShaderModule;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;
VkDescriptorPool descriptorPool;
VkDescriptorSet descriptorSet;
VkDescriptorSetLayout descriptorSetLayout;
VkBuffer buffer, buffer2;
VkDeviceMemory bufferMemory, bufferMemory2;
        
uint64_t bufferSize; // size of `buffer` in bytes.
uint64_t bufferSize2; // size of `buffer` in bytes.

VkQueue queue; // a queue supporting compute operations.
uint32_t queueFamilyIndex;


//
// total threads to start.  choosen so each call to the gpu is around 50 to 100ms.
//
const int np = 1024*1024*16;


//uint64_t K1;
//uint64_t P;
double bitlimit = 68;
uint64_t kfound[100];

void createInstance() {
        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Hello world app";
        applicationInfo.applicationVersion = 0;
        applicationInfo.pEngineName = "awesomeengine";
        applicationInfo.engineVersion = 0;
        applicationInfo.apiVersion = VK_API_VERSION_1_3;
        
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &applicationInfo;
        
        // Give our desired layers and extensions to vulkan.
        createInfo.enabledLayerCount = 0;
        //createInfo.ppEnabledLayerNames = enabledLayers.data();
        //createInfo.enabledExtensionCount = enabledExtensions.size();
        //createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    
        /*
        Actually create the instance.
        Having created the instance, we can actually start using vulkan.
        */
        VkResult res = vkCreateInstance(&createInfo, NULL, &instance);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateInstance() = %d\n", res);
	}
}

int findPhysicalDevice() {
        uint32_t deviceCount = 32;

        //std::vector<VkPhysicalDevice> devices(deviceCount);
	VkPhysicalDevice devices[32];
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	fprintf(stderr, "devices: %d\n", deviceCount);
	if (deviceCount > 0) {
		// XXX: mrh choose device, not just 0
		physicalDevice = devices[0];
	}
	return deviceCount;
}

uint32_t getComputeQueueFamilyIndex() {
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

	//fprintf(stderr, "qfc: %d\n", queueFamilyCount);
        // Retrieve all queue families.
        //std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	VkQueueFamilyProperties queueFamilies[queueFamilyCount];
	
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

	fprintf(stderr, "queueFamilyCount: %d\n", queueFamilyCount);
        // Now find a family that supports compute.

	uint32_t i = 0;
        for (; i < queueFamilyCount; ++i) {
		VkQueueFamilyProperties props = queueFamilies[i];

		if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			// found a queue with compute. We're done!
			break;
		}
        }

        if (i == queueFamilyCount) {
		fprintf(stderr, "could not find a queue family that supports operations");
        }

        return i;
}

void createDevice() {
        VkDeviceQueueCreateInfo queueCreateInfo = {};

        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueFamilyIndex = getComputeQueueFamilyIndex(); 
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1; // create one queue in this family. We don't need more.
        float queuePriorities = 1.0;  // we only have one queue, so this is not that imporant. 
        queueCreateInfo.pQueuePriorities = &queuePriorities;

        VkDeviceCreateInfo deviceCreateInfo = {};

        VkPhysicalDeviceFeatures deviceFeatures = {};

        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = 0;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        VkResult res = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateDevice() = %d\n", res);
	}

        // Get a handle to the only member of the queue family.
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
}

uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memoryProperties;
	
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if ((memoryTypeBits & (1 << i)) &&
                ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
                return i;
        }
        return -1;
}

void createBuffer() {
        
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize; // buffer size in bytes. 
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult res = vkCreateBuffer(device, &bufferCreateInfo, NULL, &buffer);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateBuffer() = %d\n", res);
	}

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
        
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size; // specify required memory.
        allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        res = vkAllocateMemory(device, &allocateInfo, NULL, &bufferMemory);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateMemory() = %d\n", res);
	}
        
        res = vkBindBufferMemory(device, buffer, bufferMemory, 0);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkBindBufferMemory() = %d\n", res);
	}
}
void createBuffer2() {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize2; 
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 

        VkResult res = vkCreateBuffer(device, &bufferCreateInfo, NULL, &buffer2);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateBuffer() = %d\n", res);
	}

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer2, &memoryRequirements);
        
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size; 
        allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        res = vkAllocateMemory(device, &allocateInfo, NULL, &bufferMemory2);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateMemory() = %d\n", res);
	}
        
        res = vkBindBufferMemory(device, buffer2, bufferMemory2, 0);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkBindBufferMemory() = %d\n", res);
	}
}
void createDescriptorSetLayout() {

        /*
        Here we specify a binding of type VK_DESCRIPTOR_TYPE_STORAGE_BUFFER to the binding point
        0. This binds to 

          layout(std140, binding = 0) buffer buf

        in the compute shader.
        */
 	VkDescriptorSetLayoutBinding b[2];
        b[0].binding = 0; // binding = 0
        b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[0].descriptorCount = 1;
        b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        b[1].binding = 1; // binding = 1
        b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[1].descriptorCount = 1;
        b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 2; // only a single binding in this descriptor set layout. 
        descriptorSetLayoutCreateInfo.pBindings = b;

        // Create the descriptor set layout. 
        VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateDescriptorSetLayout() = %d\n", res);
	}
}

void createDescriptorSet() {
        /*
        Our descriptor pool can only allocate a single storage buffer.
        */
        VkDescriptorPoolSize descriptorPoolSize = {};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorPoolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;

	VkResult res;
        // create descriptor pool.
        res = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateDescriptorPool() = %d\n", res);
	}

        /*
        With the pool allocated, we can now allocate the descriptor set. 
        */
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
        descriptorSetAllocateInfo.descriptorPool = descriptorPool; // pool to allocate from.
        descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

        // allocate descriptor set.
        res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateDescriptorSets() = %d\n", res);
	}

        /*
        Next, we need to connect our actual storage buffer with the descrptor. 
        We use vkUpdateDescriptorSets() to update the descriptor set.
        */

	VkDescriptorBufferInfo bi[2];
        // Specify the buffer to bind to the descriptor.
        //VkDescriptorBufferInfo descriptorBufferInfo = {};
        bi[0].buffer = buffer;
        bi[0].offset = 0;
        bi[0].range = bufferSize;

        bi[1].buffer = buffer2;
        bi[1].offset = 0;
        bi[1].range = bufferSize2;
	
        VkWriteDescriptorSet writeDescriptorSet = {};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet; // write to this descriptor set.
        writeDescriptorSet.dstBinding = 0; // write to the first, and only binding.
        writeDescriptorSet.descriptorCount = 2; // update a single descriptor.
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
        writeDescriptorSet.pBufferInfo = bi;

        // perform the update of the descriptor set.
        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);
}
// Read file into array of bytes, and cast to uint32_t*, then return.
// The data has been padded, so that it fits into an array uint32_t.
uint32_t* readFile(uint32_t *length, const char* filename) {

        FILE* fp = fopen(filename, "rb");
        if (fp == NULL) {
            printf("Could not find or open file: %s\n", filename);
        }

        // get file size.
        fseek(fp, 0, SEEK_END);
        long filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        long filesizepadded = (int)(ceil(filesize / 4.0)) * 4;

        // read file contents.
        //char *str = new char[filesizepadded];
	char *str = malloc(filesizepadded);
        size_t n = fread(str, filesize, sizeof(char), fp);
	//fprintf(stderr, "mrh - read %ld bytes\n", n * filesize);
        fclose(fp);

        // data padding. 
        for (int i = filesize; i < filesizepadded; i++) {
            str[i] = 0;
        }

        *length = filesizepadded;
        return (uint32_t *)str;
}

void createComputePipeline() {
        /*
        Create a shader module. A shader module basically just encapsulates some shader code.
        */
        uint32_t filelength;
        // the code in comp.spv was created by running the command:
        // glslangValidator.exe -V shader.comp
        uint32_t* code = readFile(&filelength, "comp.spv");
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pCode = code;
        createInfo.codeSize = filelength;
        
        VkResult res = vkCreateShaderModule(device, &createInfo, NULL, &computeShaderModule);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateShaderModule() = %d\n", res);
	}
        free(code);

        /*
        Now let us actually create the compute pipeline.
        A compute pipeline is very simple compared to a graphics pipeline.
        It only consists of a single stage with a compute shader. 

        So first we specify the compute shader stage, and it's entry point(main).
        */
        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageCreateInfo.module = computeShaderModule;
        shaderStageCreateInfo.pName = "main";

        /*
        The pipeline layout allows the pipeline to access descriptor sets. 
        So we just specify the descriptor set layout we created earlier.
        */
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout; 
        res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreatePipelineLayout() = %d\n", res);
	}

        VkComputePipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stage = shaderStageCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;

        /*
        Now, we finally create the compute pipeline. 
        */
	//fprintf(stderr, "mrh about to create\n");
        res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateComputePipelines() = %d\n", res);
	}
}

void createCommandBuffer() {
        /*
        We are getting closer to the end. In order to send commands to the device(GPU),
        we must first record commands into a command buffer.
        To allocate a command buffer, we must first create a command pool. So let us do that.
        */
        VkCommandPoolCreateInfo commandPoolCreateInfo = {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = 0;
        // the queue family of this command pool. All command buffers allocated from this command pool,
        // must be submitted to queues of this family ONLY. 
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        VkResult res = vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateCommandPool() = %d\n", res);
	}

        /*
        Now allocate a command buffer from the command pool. 
        */
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = commandPool; // specify the command pool to allocate from. 
        // if the command buffer is primary, it can be directly submitted to queues. 
        // A secondary buffer has to be called from some primary command buffer, and cannot be directly 
        // submitted to a queue. To keep things simple, we use a primary command buffer. 
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1; // allocate a single command buffer. 
        res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateCommandBuffers() = %d\n", res);
	}

        /*
        Now we shall start recording commands into the newly allocated command buffer. 
        */
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkBeginCommandBuffer() = %d\n", res);
	}

        /*
        We need to bind a pipeline, AND a descriptor set before we dispatch.

        The validation layer will NOT give warnings if you forget these, so be very careful not to forget them.
        */
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

        /*
        Calling vkCmdDispatch basically starts the compute pipeline, and executes the compute shader.
        The number of workgroups is specified in the arguments.
        If you are already familiar with compute shaders from OpenGL, this should be nothing new to you.
        */
	// mrh: match shader...
        vkCmdDispatch(commandBuffer, np/64, 1, 1);

        res = vkEndCommandBuffer(commandBuffer); // end recording commands.
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkEndCommandBuffer() = %d\n", res);
	}
}

void runCommandBuffer() {
        /*
        Now we shall finally submit the recorded command buffer to a queue.
        */
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        /*
          We create a fence.
        */
        VkFence fence;
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = 0;
        VkResult res = vkCreateFence(device, &fenceCreateInfo, NULL, &fence);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateFence() = %d\n", res);
	}

        /*
        We submit the command buffer on the queue, at the same time giving a fence.
        */
        res = vkQueueSubmit(queue, 1, &submitInfo, fence);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkQueueSubmit() = %d\n", res);
	}
	/*
        The command will not have finished executing until the fence is signalled.
        So we wait here.
        We will directly after this read our buffer from the GPU,
        and we will not be sure that the command has finished executing unless we wait for the fence.
        Hence, we use a fence here.
        */

        res = vkWaitForFences(device, 1, &fence, VK_TRUE, 50000000000L);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkWaitForFences() = %d\n", res);
	}

        vkDestroyFence(device, fence, NULL);
}

void cleanup() {
        /*
        Clean up all Vulkan Resources. 
        */
        vkFreeMemory(device, bufferMemory, NULL);
        vkDestroyBuffer(device, buffer, NULL);	
        vkDestroyShaderModule(device, computeShaderModule, NULL);
        vkDestroyDescriptorPool(device, descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
        vkDestroyPipelineLayout(device, pipelineLayout, NULL);
        vkDestroyPipeline(device, pipeline, NULL);
        vkDestroyCommandPool(device, commandPool, NULL);	
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);		
}

struct Stuff * mrhGetMap()  {
	void* mappedMemory = NULL;
	vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
	struct Stuff *p = (struct Stuff *) mappedMemory;
	return p;
}
void mrhUnMap()  {
	vkUnmapMemory(device, bufferMemory);
}

/*
void mrhInit(uint64_t P, uint64_t K1) {
	void* mappedMemory = NULL;
	vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
	struct Stuff *p = (struct Stuff *) mappedMemory;
	//printf("--K: %ld\n", K1);
	K1 = M * (K1/M);
	//printf("++K: %ld\n", K1);
	p->K[0] = K1 & 0xffffffff;
	p->K[1] = (K1>>32) & 0xffffffff;
	p->K[2] = 0;
	p->P[0] = P & 0xffffffff;
	p->P[1] = (P>>32) & 0xffffffff;
	for (int i = 0; i < 10; i++) {
		p->Found[i][0] = 0;
		p->Found[i][1] = 0;
		p->Found[i][2] = 0;
	}
	p->Debug[0] = p->Debug[1] = 0;
	p->Init = 0;

	for (uint64_t i = 0; i < M2; i++) {
		uint64_t q = ((uint64_t)(P % M2)) * 2 * i + 1;
		if ((q % 29) == 0 ||(q % 31) == 0 ||(q % 37) == 0||(q % 41) == 0 || (q % 43) == 0) {
			p->X2[i] = 0;
		} else {
			p->X2[i] = 1;
		}
	}

	//
	// This takes a second or so on the cpu, but is so much easier with 64bit ints.
	//
	unsigned int ones = 0;
	for (uint64_t i = 0; i < M; i++) {
		uint64_t q = ((uint64_t)(P % M)) * 2 * i + 1;
		if (((q&7) == 3) || ((q&7) == 5) || (q%3 == 0) || (q%5 == 0) || (q%7 == 0) || (q%11 == 0) ||
		    (q%13 == 0) || (q%17 == 0) || (q%19 == 0) || (q%23 == 0)) {
			//Kx[i] = 0;
		} else {
			p->List[ones] = i;
			ones++;
			if (ones > ListLen) {
				printf("Error: list longer than expected: %d.  %ld isn't prime??\n", ones, P);
				exit(1);
			}
		}
	}
	p->L = 0;
	p->Ll = ones;
	//printf("init: %d - %0.3f sieved\n", ones, double(ones)/M);
	//exit(1);
	runCommandBuffer();  // initialize some shader stuff
	//printf("init done\n");

	p->Init = 1;         // now we are done.
	p->L = 0;

	vkUnmapMemory(device, bufferMemory);
}

void mrhRun(uint64_t P, uint64_t K1) {
	uint32_t mrhDone = 0;
	void* mappedMemory = NULL;
	vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
	struct Stuff *p = (struct Stuff *) mappedMemory;

	K1 = M * (K1/M);

	uint64_t t = M * (P%M) * 2;
	printf("M * PmodM * 2 = %lx\n", t);
	uint64_t k64 = K1;
	int calls = 0;
	int found = 0;
	while (1) {
		
		struct timeval t1, t2;
		gettimeofday(&t1, NULL);

		p->Debug[0] = 0;
		p->Debug[1] = 0;
		//printf("K mod M2: %ld\n", k64 % M2);
		p->KmodM2 = (uint32_t)(k64 % M2);
		runCommandBuffer();
		p->Init++;
		calls++;

		gettimeofday(&t2, NULL);
		double elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
		elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;

		double dk64 = k64;
		double dP = P;
		double lb2 = log2(dk64 * dP * 2.0);
		if (lb2 > bitlimit) {
			mrhDone = 1;
		}

		for (int i = 0; i < 10; i++) {
			uint64_t f64 = p->Found[i][1];
			f64 <<= 32;
			f64 |= p->Found[i][0];
			if (f64) {
				kfound[found++] = f64;

				double flb2 = log2((double)f64 * dP * 2.0);
				printf("# %ld kfactor %ld E: %d D: %d %.1f\n", P, f64, p->Debug[0], p->Debug[1], flb2);

				p->Found[i][0] = 0;
				p->Found[i][1] = 0;
				//mrhDone = 1;
			}
		}

		if (mrhDone) {
			time_t t ;
			time(&t);
			printf("%ld %ld %ld %.2f %d ", P, t, k64, bitlimit, found);
			for (int i = 0; i < found; i++) {
				printf("%ld ", kfound[i]);
			}		       
			printf("\n");
			break;
		}

		if (p->L >= p->Ll) {
			k64 += M;
			p->K[0] = (k64) & 0xffffffff;
			p->K[1] = (k64)>>32;
			p->K[2] = 0;

			p->L = 0;
			p->Debug[0] = p->Debug[1] = 0;
			p->Init = 1;
		}
						       
    
	}
}
*/

int tfVulkanInit() {
	bufferSize = sizeof(struct Stuff);
	bufferSize2 = sizeof(struct Stuff2);

        // Initialize vulkan:
	createInstance();
	int devices = findPhysicalDevice();
	if (devices == 0) {return -1;}
        createDevice();
        createBuffer();
        createBuffer2();
        createDescriptorSetLayout();
        createDescriptorSet();
        createComputePipeline();

	createCommandBuffer();
	return 0;

	//mrhInit();
	//mrhRun();
	//cleanup();
}

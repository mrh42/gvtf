#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "tf.h"
#include "spv32.h"
#include "spv64.h"

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
// A lot of code in this file was inspired by:
//    https://github.com/Erkaman/vulkan_minimal_compute
//
// total threads to start.
//
//const int np = 1024*1024*8;
const int NP = 1024*512;
const int XSIZE = 64;

int createInstance() {
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
		return -1;
	}
	return 0;
}

int findPhysicalDevice(int d) {
        uint32_t deviceCount = 32;

	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	VkPhysicalDevice devices[deviceCount];
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	if (deviceCount > 0) {
		if (d > deviceCount-1) {
			d = deviceCount-1;
		}
		physicalDevice = devices[d];

		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(physicalDevice, &p);
		fprintf(stdout, "# findPhysicalDevice(): count: %d, selected: %d name: '%s' type: %d\n",
			deviceCount, d, p.deviceName, p.deviceType);
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

	//fprintf(stderr, "queueFamilyCount: %d\n", queueFamilyCount);
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
        //allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

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
	    return 0;
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

void createComputePipeline(const uint32_t *code, uint32_t codesize) {
        /*
        Create a shader module. A shader module basically just encapsulates some shader code.
        */
	/*
        uint32_t filelength;
        // the code in comp.spv was created by running the command:
        // glslangValidator.exe -V shader.comp
        uint32_t* code = readFile(&filelength, "comp.spv");
	*/

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pCode = code;
        createInfo.codeSize = codesize;
        
        VkResult res = vkCreateShaderModule(device, &createInfo, NULL, &computeShaderModule);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateShaderModule() = %d\n", res);
	}
        //free(code);

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
        vkCmdDispatch(commandBuffer, NP/XSIZE, 1, 1);

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

//struct Stuff * mrhGetMap()  {
void * mrhGetMap()  {
	void* mappedMemory = NULL;
	vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
	return mappedMemory;
}
void mrhUnMap()  {
	vkUnmapMemory(device, bufferMemory);
}

int tfVulkanInit(int devn, uint64_t bs1, uint64_t bs2, int version) {
	bufferSize = bs1;
	bufferSize2 = bs2;

	uint32_t codesize;
	const uint32_t* code;

	int needfree = 0;
	if (version == 32) {
		codesize = sizeof(spv32);
		code = spv32;
	} else if (version == 64) {
		codesize = sizeof(spv64);
		code = spv64;
	} else {
		needfree = 1;
		code = readFile(&codesize, "comp.spv");
		if (code == 0) {return -1;}
	}

        // Initialize vulkan:
	if (createInstance() < 0) {return -1;}
	int devices = findPhysicalDevice(devn);
	if (devices == 0) {return -1;}

        createDevice();
	//fprintf(stderr, "created device\n");
        createBuffer();
	//fprintf(stderr, "created buffer1\n");
        createBuffer2();
	//fprintf(stderr, "created buffer2\n");
        createDescriptorSetLayout();
	//fprintf(stderr, "created dslo\n");
        createDescriptorSet();
	//fprintf(stderr, "created ds -- %d\n", codesize);
        createComputePipeline(code, codesize);
	//fprintf(stderr, "created pipeline\n");

	createCommandBuffer();

	if (needfree) {free((void*)code);}
	return 0;
}

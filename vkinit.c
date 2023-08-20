#define VK_USE_PLATFORM_XLIB_KHR

#include <vulkan/vulkan.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern Display *d;
extern Window w;

void mkwin(int wi, int he, char *l);
void readev(void);

VkInstance inst;
VkPhysicalDevice pdev; /* should be local? */
VkDevice dev;
VkQueue gque,pque;
VkSurfaceKHR sur;
VkSwapchainKHR sc;
int imgn;
VkImageView *imgviews;
VkShaderModule vert, frag;
VkPipelineLayout playout;
VkRenderPass rpass;
VkPipeline pl;
VkFramebuffer *fbs;
VkCommandPool pool;
int qfam;
VkCommandBuffer cbuf;
VkSemaphore simg, srender;
VkFence fence;

static void
exits(char *s)
{
	puts(s);
	exit(0);
}

void
vkinit(void)
{
	VkResult res;

	/* ----------------------------	*/
	/* INSTANCE CREATION 			*/
	/* ----------------------------	*/

	int extn = 2; /* in case more extensions are needed */
	const char *instext[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME
	};

	VkApplicationInfo appinfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "name",
		.applicationVersion = VK_MAKE_VERSION(1,0,0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1,0,0),
		.apiVersion = VK_API_VERSION_1_0
	};

	VkInstanceCreateInfo instinfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appinfo,
		.enabledExtensionCount = extn,
		.ppEnabledExtensionNames = instext,
		.enabledLayerCount = 0
	};

	res = vkCreateInstance(&instinfo,0,&inst);	
	if(res!=VK_SUCCESS)
		exits("can't create instance");

	/* ----------------------------	*/
	/* SURFACE CREATION 			*/
	/* ----------------------------	*/

	VkXlibSurfaceCreateInfoKHR surinfo = {
		.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy = d,
		.window = w
	};
	res = vkCreateXlibSurfaceKHR(inst,&surinfo,0,&sur);
	if(res!=VK_SUCCESS)
		exits("can't create xlib surface");

	
	/* ----------------------------	*/
	/* PHYSICAL DEVICE CREATION 	*/
	/* ----------------------------	*/

	int n_qfam,pres_sup;

	int pdevn;
	res = vkEnumeratePhysicalDevices(inst,&pdevn,0);
	if(pdevn==0 || res!=VK_SUCCESS)
		exits("can't find any physical devices");
	VkPhysicalDevice pdevs[pdevn];
	vkEnumeratePhysicalDevices(inst,&pdevn,pdevs);

	VkPhysicalDeviceProperties prop = {};
	VkPhysicalDeviceFeatures feat = {};

	pdev = 0;

	int i,j;	
	for(i=0; i<pdevn; ++i){
		vkGetPhysicalDeviceFeatures(pdevs[i],&feat);
		vkGetPhysicalDeviceProperties(pdevs[i],&prop);

		vkEnumerateDeviceExtensionProperties(pdevs[i],0,&extn,0);
		VkExtensionProperties a_devext[extn];
		vkEnumerateDeviceExtensionProperties(pdevs[i],0,&extn,a_devext);

		int sc_sup=0;
		char *ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		for(j=0; j<extn; ++j){
			if(strncmp(a_devext[j].extensionName,ext,strlen(ext))==0)
				sc_sup=1;
		}
		if(sc_sup==0)
			exits("swapchain isn't supported");

		/* check swapchain format and presentation modes */

		vkGetPhysicalDeviceQueueFamilyProperties(pdevs[i],&n_qfam,0);
		VkQueueFamilyProperties a_qfam[n_qfam];
		vkGetPhysicalDeviceQueueFamilyProperties(pdevs[i],&n_qfam,a_qfam);
		for(j=0; j<n_qfam; ++j){
			if(a_qfam[j].queueFlags & VK_QUEUE_GRAPHICS_BIT){
				vkGetPhysicalDeviceSurfaceSupportKHR(pdevs[i],j,sur,&pres_sup);
				if(pres_sup){
					qfam = j;
					pdev = pdevs[i];
					break;
				}
			}
		}
	}

	if(pdev==0)
		exits("can't find physical device");
	printf("found %d physical device(s)\n", pdevn);
	printf("chose physical device called %s\n", prop.deviceName);

	/* ----------------------------	*/
	/* LOGICAL DEVICE CREATION 		*/
	/* ----------------------------	*/

	float p=1;
	VkDeviceQueueCreateInfo queinfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = qfam,
		.queueCount = 1, /* multi-threading? */
		.pQueuePriorities = &p
	};

	extn = 1; /* in case more extensions are needed */
	const char *devext[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo devinfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pQueueCreateInfos = &queinfo,
		.queueCreateInfoCount = 1,
		.pEnabledFeatures = &feat,
		.enabledExtensionCount = extn,
		.ppEnabledExtensionNames = devext,
		.enabledLayerCount = 0
	};

	res = vkCreateDevice(pdev,&devinfo,0,&dev);
	if(res!=VK_SUCCESS)
		exits("can't create device");

	vkGetDeviceQueue(dev,qfam,0,&gque); /* same queue? */
	/* yes, same */
	pque = gque;


	/* ----------------------------	*/
	/* SWAP CHAIN CREATION			*/
	/* ----------------------------	*/


	/* surface capabilities */
	VkSurfaceCapabilitiesKHR scap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdev,sur,&scap);

	/* surface formats */
	int fmn;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pdev,sur,&fmn,0);
	VkSurfaceFormatKHR fms[fmn];
	vkGetPhysicalDeviceSurfaceFormatsKHR(pdev,sur,&fmn,fms);

	/* presentation modes */
	int pmn;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pdev,sur,&pmn,0);
	VkPresentModeKHR pms[pmn];
	vkGetPhysicalDeviceSurfacePresentModesKHR(pdev,sur,&pmn,pms);


	VkExtent2D ex;
	VkSurfaceFormatKHR fm;

	VkSwapchainCreateInfoKHR scinfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = sur,
		.minImageCount = 0,
		.imageFormat = VK_FORMAT_R8G8B8A8_SRGB, /* choose format! */
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR, 
		.imageExtent = (VkExtent2D){800,600}, /* create extent */
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = (int[]){qfam},
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_MAILBOX_KHR /* choose present mode! */
	};
	
	res = vkCreateSwapchainKHR(dev,&scinfo,0,&sc);
	if(res!=VK_SUCCESS)
		exits("can't create swap chain");

	vkGetSwapchainImagesKHR(dev,sc,&imgn,0);
	VkImage imgs[imgn];
	vkGetSwapchainImagesKHR(dev,sc,&imgn,imgs);


	/* ----------------------------	*/
	/* IMAGE VIEWS					*/
	/* ----------------------------	*/

	imgviews = (VkImageView*)malloc(sizeof(VkImageView) * imgn);
	for(i=0; i<imgn; ++i){
		VkImageViewCreateInfo imgviewinfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = imgs[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_SRGB, /* choose it */
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};
		res = vkCreateImageView(dev,&imgviewinfo,0,&imgviews[i]);
		if(res!=VK_SUCCESS)
			exits("can't create image view");
	}
}

void
vkhalt(void)
{
	vkDeviceWaitIdle(dev);
	vkDestroyFence(dev,fence,0);
	vkDestroySemaphore(dev,srender,0);
	vkDestroySemaphore(dev,simg,0);
	vkDestroyCommandPool(dev,pool,0);
	int i;
	for(i=0; i<imgn; ++i){
		vkDestroyFramebuffer(dev,fbs[i],0);
	}
	vkDestroyPipeline(dev,pl,0);
	vkDestroyRenderPass(dev,rpass,0);
	vkDestroyPipelineLayout(dev,playout,0);
	vkDestroyShaderModule(dev,frag,0);
	vkDestroyShaderModule(dev,vert,0);
	for(i=0; i<imgn; ++i){
		vkDestroyImageView(dev,imgviews[i],0);
	}
	vkDestroySwapchainKHR(dev,sc,0);
	vkDestroySurfaceKHR(inst,sur,0);
	vkDestroyDevice(dev,0);
	vkDestroyInstance(inst,0);
}

VkShaderModule
shader(char *path)
{
	FILE *f = fopen(path,"r");
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	fseek(f,0,SEEK_SET);
	uint *code = calloc(size,1);
	fread(code,4,size/4,f);
	printf("size = %d\n",size);

	VkShaderModule shader;

	VkShaderModuleCreateInfo shinfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = code,
	};
	VkResult res = vkCreateShaderModule(dev,&shinfo,0,&shader);
	if(res!=VK_SUCCESS)
		exits("failed to create shader module");

	free(code);
	return shader;
}

void
triangle(void)
{
	int i;

	/* ----------------------------	*/
	/* SHADERS						*/
	/* ----------------------------	*/

	vert = shader("res/vert.spv");
	frag = shader("res/frag.spv");

	VkPipelineShaderStageCreateInfo vertinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo fraginfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo shaderinfos[] = {
		vertinfo, fraginfo
	};

	/* ----------------------------	*/
	/* DYNAMIC STATES				*/
	/* ----------------------------	*/

	VkDynamicState dyn[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dyninfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dyn
	};

	/* ----------------------------	*/
	/* VERTEX BUFFER				*/
	/* ----------------------------	*/

	VkPipelineVertexInputStateCreateInfo vbinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = 0,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = 0
	};

	VkPipelineInputAssemblyStateCreateInfo inputinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = 0
	};

	/* ----------------------------	*/
	/* VIEWPORT 					*/
	/* ----------------------------	*/

/*
	VkViewport vp = {
		.x = 0,
		.y = 0,
		.width = 800,
		.height = 600,
		.minDepth = 0,
		.maxDepth = 1
	};

	VkRect2D scissor = {
		.offset = {0,0},
		.extent = (VkExtent2D){800,600}
	};
*/

	VkPipelineViewportStateCreateInfo vpinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineRasterizationStateCreateInfo rastinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = 0,
		.rasterizerDiscardEnable = 0,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = 0,
		.depthBiasConstantFactor = 0,
		.depthBiasClamp = 0,
		.depthBiasSlopeFactor = 0
	};

	/* disabled (needs gpu feature) */
	VkPipelineMultisampleStateCreateInfo msinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1,
		.pSampleMask = 0,
		.alphaToCoverageEnable = 0,
		.alphaToOneEnable = 0
	};

	/* depth buffering */

	/* colour blending */

	VkPipelineColorBlendAttachmentState cbstate = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = 0,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD
	};

	VkPipelineColorBlendStateCreateInfo cbinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = 0,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &cbstate,
		.blendConstants = {
			0,0,0,0
		}
	};

	/* ----------------------------	*/
	/* PIPELINE LAYOUT				*/
	/* ----------------------------	*/

	VkPipelineLayoutCreateInfo playinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pSetLayouts = 0,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = 0
	};

	int res = vkCreatePipelineLayout(dev,&playinfo,0,&playout);
	if(res!=VK_SUCCESS)
		exits("can't create pipeline layout");

	/* ----------------------------	*/
	/* RENDER PASS					*/
	/* ----------------------------	*/

	VkAttachmentDescription desc = {
		.format = VK_FORMAT_R8G8B8A8_SRGB, /* SWAP CHAIN FORMAT */
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription sub = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &ref
	};

	VkSubpassDependency dep = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo rpassinfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &desc,
		.subpassCount = 1,
		.pSubpasses = &sub,
		.dependencyCount = 1,
		.pDependencies = &dep
	};

	res = vkCreateRenderPass(dev,&rpassinfo,0,&rpass);
	if(res!=VK_SUCCESS)
		exits("can't create render pass");


	/* ----------------------------	*/
	/* PIPELINE						*/
	/* ----------------------------	*/

	VkGraphicsPipelineCreateInfo plinfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderinfos,
		.pVertexInputState = &vbinfo,
		.pInputAssemblyState = &inputinfo,
		.pViewportState = &vpinfo,
		.pRasterizationState = &rastinfo,
		.pMultisampleState = &msinfo,
		.pDepthStencilState = 0, /* yet */
		.pColorBlendState = &cbinfo,
		.pDynamicState = &dyninfo,
		.layout = playout,
		.renderPass = rpass,
		.subpass = 0,
		.basePipelineHandle = 0,
		.basePipelineIndex = -1
	};

	res = vkCreateGraphicsPipelines(dev,0,1,&plinfo,0,&pl);
	if(res!=VK_SUCCESS)
		exits("can't create pipeline");


	/* ----------------------------	*/
	/* FRAMEBUFFERS					*/
	/* ----------------------------	*/

	fbs = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * imgn);
	for(i=0; i<imgn; ++i){
		VkImageView imgview[] = {
			imgviews[i]
		};

		VkFramebufferCreateInfo fbinfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = rpass,
			.attachmentCount = 1,
			.pAttachments = imgview,
			.width = 800,
			.height = 600,
			.layers = 1
		};

		res = vkCreateFramebuffer(dev,&fbinfo,0,&fbs[i]);
		if(res!=VK_SUCCESS)
			exits("can't create frame buffer");
	}

	/* ----------------------------	*/
	/* COMMAND POOL					*/
	/* ----------------------------	*/

	VkCommandPoolCreateInfo poolinfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = qfam
	};

	res = vkCreateCommandPool(dev,&poolinfo,0,&pool);
	if(res!=VK_SUCCESS)
		exits("can't create command pool");


	/* ----------------------------	*/
	/* COMMAND BUFFER				*/
	/* ----------------------------	*/

	VkCommandBufferAllocateInfo cbufinfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	res = vkAllocateCommandBuffers(dev,&cbufinfo,&cbuf);
	if(res!=VK_SUCCESS)
		exits("can't allocate command buffer");


	/* ----------------------------	*/
	/* COMMANDS 					*/
	/* ----------------------------	*/
/*
	VkCommandBufferBeginInfo begininfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = 0
	};

	res = vkBeginCommandBuffer(cbuf,&begininfo);
	if(res!=VK_SUCCESS)
		exits("can't begin command buffer");

	VkClearValue col = {{{0,0,0,1}}};

	VkRenderPassBeginInfo rbegininfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = rpass,
		.framebuffer = fbs[],
		.renderArea = {
			.offset = {0,0}
			.extent = (VkExtent2D){800,600},
		}
		.clearValueCount = 1,
		.pClearValues = &col
	};

	vkCmdBeginRenderPass(cbuf,&rbegininfo,VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cbuf,VK_PIPELINE_BIND_POINT_GRAPHICS,pl);
	vkCmdSetViewport(cbuf,0,1,&vp);
	vkCmdSetScissor(cbuf,0,1,&scissor);
	vkCmdDraw(cbuf,3,1,0,0);
	vkCmdEndRenderPass(cbuf);

	res = vkEndCommandBuffer(cbuf);
	if(res!=VK_SUCCESS)
		exits("can't end command buffer");
*/

	/* ----------------------------	*/
	/* SEMAPHORES AND FENCE			*/
	/* ----------------------------	*/

	VkSemaphoreCreateInfo seminfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo feninfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	res = vkCreateSemaphore(dev,&seminfo,0,&simg);
	if(res!=VK_SUCCESS)
		exits("can't create semaphore");
	res = vkCreateSemaphore(dev,&seminfo,0,&srender);
	if(res!=VK_SUCCESS)
		exits("can't create semaphore");

	res = vkCreateFence(dev,&feninfo,0,&fence);
		if(res!=VK_SUCCESS)
		exits("can't create fence");
}

void
draw(void)
{
	int res;

	vkWaitForFences(dev,1,&fence,1,UINT64_MAX);
	vkResetFences(dev,1,&fence);

	int img;
	vkAcquireNextImageKHR(dev,sc,UINT64_MAX,simg,0,&img);

	vkResetCommandBuffer(cbuf,0);

	VkCommandBufferBeginInfo begininfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = 0
	};

	res = vkBeginCommandBuffer(cbuf,&begininfo);
	if(res!=VK_SUCCESS)
		exits("can't begin command buffer");

	VkClearValue col = {{{0,0,0,1}}};

	VkRenderPassBeginInfo rbegininfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = rpass,
		.framebuffer = fbs[img],
		.renderArea = {
			.offset = {0,0},
			.extent = (VkExtent2D){800,600}, /* CREATE EXTENT!!! */
		},
		.clearValueCount = 1,
		.pClearValues = &col
	};

	vkCmdBeginRenderPass(cbuf,&rbegininfo,VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cbuf,VK_PIPELINE_BIND_POINT_GRAPHICS,pl);

	VkViewport vp = {
		.x = 0,
		.y = 0,
		.width = 800, /* should be from swapchain */
		.height = 600,
		.minDepth = 0,
		.maxDepth = 1
	};

	VkRect2D scissor = {
		.offset = {0,0},
		.extent = (VkExtent2D){800,600} /* should be from swap chain */
	};

	vkCmdSetViewport(cbuf,0,1,&vp);
	vkCmdSetScissor(cbuf,0,1,&scissor);
	vkCmdDraw(cbuf,3,1,0,0);
	vkCmdEndRenderPass(cbuf);

	res = vkEndCommandBuffer(cbuf);
	if(res!=VK_SUCCESS)
		exits("can't end command buffer");

	VkSemaphore wait[] = {simg};
	VkPipelineStageFlags wstages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore sig[] = {srender};

	VkSubmitInfo submitinfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = wait,
		.pWaitDstStageMask = wstages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cbuf,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = sig
	};

	res = vkQueueSubmit(gque,1,&submitinfo,fence);
	if(res!=VK_SUCCESS)
		exits("can't submit queue");

	VkSwapchainKHR scs[] = {sc};

	VkPresentInfoKHR presinfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = sig,
		.swapchainCount = 1,
		.pSwapchains = scs,
		.pImageIndices = &img,
		.pResults = 0
	};

	vkQueuePresentKHR(pque,&presinfo);
}

int
main(void)
{
	mkwin(800,600,"test");

	vkinit();

	triangle();

	puts("great success!!!");

	for(;;){
		readev();
		draw();
	}

	vkhalt();

	return 0;
}


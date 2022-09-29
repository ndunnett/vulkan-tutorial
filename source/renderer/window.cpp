#include "window.h"

namespace tutorial {
    FrameTransients::FrameTransients(VulkanCore* vulkan, size_t frames_in_flight)
        : vulkan(vulkan), m_frames(frames_in_flight) {
        vk::CommandBufferAllocateInfo ai{};
        ai.setCommandPool(vulkan->get_command_pool());
        ai.setLevel(vk::CommandBufferLevel::ePrimary);
        ai.setCommandBufferCount(m_frames.size());
        auto command_buffers = vulkan->get_logical_device().allocateCommandBuffersUnique(ai);

        for (size_t i = 0; i < m_frames.size(); i++) {
            m_frames.at(i).image_available = vulkan->get_logical_device().createSemaphoreUnique({});
            m_frames.at(i).render_finished = vulkan->get_logical_device().createSemaphoreUnique({});
            m_frames.at(i).in_flight =
                vulkan->get_logical_device().createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
            m_frames.at(i).command_buffer = std::move(command_buffers.at(i));
        }
    }

    void FrameTransients::wait_for_fences() {
        if (vulkan->get_logical_device().waitForFences(*m_frames.at(m_frame_index).in_flight, VK_TRUE,
                                                       UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to wait for frame fences!");
        }
    }

    void FrameTransients::reset_fences() {
        vulkan->get_logical_device().resetFences(*m_frames.at(m_frame_index).in_flight);
    }

    vk::ResultValue<uint32_t> FrameTransients::next_image_index(const vk::SwapchainKHR& swapchain) {
        auto index = vulkan->get_logical_device().acquireNextImageKHR(
            swapchain, UINT64_MAX, *m_frames.at(m_frame_index).image_available);

        if (index.result != vk::Result::eSuccess && index.result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swapchain image!");
        }

        return index;
    }

    void FrameTransients::reset_command_buffer() {
        m_frames.at(m_frame_index).command_buffer->reset();
    }

    void FrameTransients::submit(const vk::Queue& queue, vk::PipelineStageFlags dst_stage_mask) {
        vk::SubmitInfo si{};
        si.setWaitSemaphores(*m_frames.at(m_frame_index).image_available);
        si.setWaitDstStageMask(dst_stage_mask);
        si.setCommandBuffers(*m_frames.at(m_frame_index).command_buffer);
        si.setSignalSemaphores(*m_frames.at(m_frame_index).render_finished);
        queue.submit(si, *m_frames.at(m_frame_index).in_flight);
    }

    vk::Result FrameTransients::present(const vk::Queue& queue, const vk::SwapchainKHR& swapchain,
                                        uint32_t index) {
        vk::PresentInfoKHR pi{};
        pi.setWaitSemaphores(*m_frames.at(m_frame_index).render_finished);
        pi.setSwapchains(swapchain);
        pi.setImageIndices(index);

        auto result = queue.presentKHR(pi);

        if (result != vk::Result::eSuccess && result != vk::Result::eErrorOutOfDateKHR &&
            result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to present swapchain image!");
        }

        return result;
    }

    ImageResource::ImageResource(VulkanCore* vulkan, const ImageProperties& properties)
        : vulkan(vulkan), properties(properties) {
        vk::Extent3D extent{ properties.size.first, properties.size.second, 1 };

        vk::ImageCreateInfo image_ci{};
        image_ci.setImageType(vk::ImageType::e2D);
        image_ci.setExtent(extent);
        image_ci.setMipLevels(properties.mip_levels);
        image_ci.setArrayLayers(1);
        image_ci.setFormat(properties.format);
        image_ci.setTiling(properties.tiling);
        image_ci.setInitialLayout(vk::ImageLayout::eUndefined);
        image_ci.setUsage(properties.usage);
        image_ci.setSharingMode(vk::SharingMode::eExclusive);
        image_ci.setSamples(properties.samples);

        image = vulkan->get_logical_device().createImageUnique(image_ci);
        auto mem_req = vulkan->get_logical_device().getImageMemoryRequirements(*image);

        vk::MemoryAllocateInfo image_ai{};
        image_ai.setAllocationSize(mem_req.size);
        image_ai.setMemoryTypeIndex(vulkan->find_memory_type(mem_req.memoryTypeBits, properties.memory));

        memory = vulkan->get_logical_device().allocateMemoryUnique(image_ai);
        vulkan->get_logical_device().bindImageMemory(*image, *memory, 0);

        vk::ImageViewCreateInfo view_ci{};
        view_ci.setImage(*image);
        view_ci.setViewType(vk::ImageViewType::e2D);
        view_ci.setFormat(properties.format);
        view_ci.setComponents({ {}, {}, {}, {} });
        view_ci.setSubresourceRange({ properties.aspect_flags, 0, properties.mip_levels, 0, 1 });

        view = vulkan->get_logical_device().createImageViewUnique(view_ci);
    }

    void ImageResource::transition_layout(const vk::Queue& queue, vk::ImageLayout old_layout,
                                          vk::ImageLayout new_layout) {
        auto commands = vulkan->get_single_time_commands(queue);

        vk::ImageMemoryBarrier barrier{};
        barrier.setOldLayout(old_layout);
        barrier.setNewLayout(new_layout);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setImage(*image);
        barrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, properties.mip_levels, 0, 1 });

        vk::PipelineStageFlags source_stage{};
        vk::PipelineStageFlags destination_stage{};

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.setSrcAccessMask({});
            barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
                   new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
            barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        } else if (old_layout == vk::ImageLayout::eUndefined &&
                   new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.setSrcAccessMask({});
            barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                     vk::AccessFlagBits::eDepthStencilAttachmentWrite);
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

            // has stencil component
            if (properties.format == vk::Format::eD32SfloatS8Uint ||
                properties.format == vk::Format::eD24UnormS8Uint) {
                barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        } else {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        commands.get_buffer().pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
    }

    Window::Window(VulkanCore* vulkan, std::string_view title, std::pair<int, int> size,
                   const std::vector<std::pair<int, int>>& hints)
        : vulkan(vulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        for (auto hint : hints) {
            glfwWindowHint(hint.first, hint.second);
        }

        m_window = glfwCreateWindow(size.first, size.second, title.data(), nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);

        if (!m_window) {
            throw std::runtime_error("failed to create GLFW window!");
        }

        m_surface = create_surface();
        vulkan->initialise_devices(*m_surface);
        // m_msaa_samples = vulkan->get_max_msaa_samples();
        std::tie(m_graphics_queue, m_present_queue) = vulkan->get_queues();
        get_swapchain_details();
        m_swapchain = create_swapchain();
        m_swapchain_images = vulkan->get_logical_device().getSwapchainImagesKHR(*m_swapchain);
        m_swapchain_views = create_swapchain_views();
        m_render_pass = create_render_pass();
        m_pipeline_layout = create_pipeline_layout();
        m_graphics_pipeline = create_graphics_pipeline();
        m_color_image = create_color_image();
        m_depth_image = create_depth_image();
        m_framebuffers = create_framebuffers();
        m_frames = std::make_unique<FrameTransients>(vulkan, 2);
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
    }

    void Window::set_extent(std::pair<uint32_t, uint32_t> size) {
        m_extent.width = std::clamp(size.first, m_support.capabilities.minImageExtent.width,
                                    m_support.capabilities.maxImageExtent.width);
        m_extent.height = std::clamp(size.second, m_support.capabilities.minImageExtent.height,
                                     m_support.capabilities.maxImageExtent.height);
    }

    void Window::rebuild_swapchain() {
        auto size = get_window_size<uint32_t>();
        vulkan->get_logical_device().waitIdle();
        m_framebuffers.clear();
        m_swapchain_views.clear();
        m_swapchain_images.clear();
        set_extent(size);
        get_swapchain_details();
        m_swapchain = create_swapchain(*m_swapchain);
        m_swapchain_images = vulkan->get_logical_device().getSwapchainImagesKHR(*m_swapchain);
        m_swapchain_views = create_swapchain_views();
        m_color_image = create_color_image();
        m_depth_image = create_depth_image();
        m_framebuffers = create_framebuffers();
    }

    void Window::draw_frame() {
        m_frames->wait_for_fences();

        auto index = m_frames->next_image_index(*m_swapchain);

        if (index.result == vk::Result::eErrorOutOfDateKHR) {
            rebuild_swapchain();
            return;
        }

        m_frames->reset_fences();
        m_frames->reset_command_buffer();
        record_command_buffer(index.value);
        m_frames->submit(m_graphics_queue, vk::PipelineStageFlagBits::eColorAttachmentOutput);

        auto present = m_frames->present(m_present_queue, *m_swapchain, index.value);

        if (present == vk::Result::eErrorOutOfDateKHR || present == vk::Result::eSuboptimalKHR ||
            m_framebuffer_resized) {
            m_framebuffer_resized = false;
            rebuild_swapchain();
        }

        m_frames->next_frame();
    }

    vk::UniqueSurfaceKHR Window::create_surface() const {
        VkSurfaceKHR raw_surface{};

        if (glfwCreateWindowSurface(vulkan->get_instance(), m_window, nullptr, &raw_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        return vk::UniqueSurfaceKHR(raw_surface, vulkan->get_deleter());
    }

    vk::UniqueShaderModule Window::create_shader_module(const ShaderSource& shader_source) const {
        auto compiled_shader = compile_shader(shader_source);
        return vulkan->get_logical_device().createShaderModuleUnique({ {}, compiled_shader });
    }

    vk::UniqueRenderPass Window::create_render_pass() const {
        vk::AttachmentDescription color_attachment{};
        color_attachment.setFormat(m_surface_format.format);
        color_attachment.setSamples(m_msaa_samples);
        color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
        color_attachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference color_attachment_ref{};
        color_attachment_ref.setAttachment(0);
        color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // vk::AttachmentDescription depth_attachment{};
        // depth_attachment.setFormat(vulkan->find_depth_format());
        // depth_attachment.setSamples(m_msaa_samples);
        // depth_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        // depth_attachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
        // depth_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        // depth_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        // depth_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
        // depth_attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // vk::AttachmentReference depth_attachment_ref{};
        // depth_attachment_ref.setAttachment(1);
        // depth_attachment_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // vk::AttachmentDescription color_attachment_resolve{};
        // color_attachment_resolve.setFormat(m_surface_format.format);
        // color_attachment_resolve.setSamples(vk::SampleCountFlagBits::e1);
        // color_attachment_resolve.setLoadOp(vk::AttachmentLoadOp::eDontCare);
        // color_attachment_resolve.setStoreOp(vk::AttachmentStoreOp::eStore);
        // color_attachment_resolve.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        // color_attachment_resolve.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        // color_attachment_resolve.setInitialLayout(vk::ImageLayout::eUndefined);
        // color_attachment_resolve.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        // vk::AttachmentReference color_attachment_resolve_ref{};
        // color_attachment_resolve_ref.setAttachment(2);
        // color_attachment_resolve_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass{};
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subpass.setColorAttachments(color_attachment_ref);
        // subpass.setPDepthStencilAttachment(&depth_attachment_ref);
        // subpass.setResolveAttachments(color_attachment_resolve_ref);

        vk::SubpassDependency dependency{};
        dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
        dependency.setDstSubpass(0);
        dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        dependency.setSrcAccessMask(vk::AccessFlagBits::eNone);
        dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
        // dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
        // dependency.setDstSubpass(0);
        // dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        //                            vk::PipelineStageFlagBits::eEarlyFragmentTests);
        // dependency.setSrcAccessMask(vk::AccessFlagBits::eNone);
        // dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        //                            vk::PipelineStageFlagBits::eEarlyFragmentTests);
        // dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
        //                             vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        // const std::array attachments{ color_attachment, depth_attachment, color_attachment_resolve };

        vk::RenderPassCreateInfo render_pass_ci{};
        render_pass_ci.setAttachments(color_attachment);
        render_pass_ci.setSubpasses(subpass);
        render_pass_ci.setDependencies(dependency);

        return vulkan->get_logical_device().createRenderPassUnique(render_pass_ci);
    }

    vk::UniquePipelineLayout Window::create_pipeline_layout() const {
        vk::PipelineLayoutCreateInfo pipeline_layout_ci{};
        return vulkan->get_logical_device().createPipelineLayoutUnique(pipeline_layout_ci);
    }

    vk::UniquePipeline Window::create_graphics_pipeline() const {
        auto fragment_shader = create_shader_module(fragment_shader_source);
        auto vertex_shader = create_shader_module(vertex_shader_source);

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_cis;
        shader_stage_cis.emplace_back(vk::PipelineShaderStageCreateInfo(
            {}, vk::ShaderStageFlagBits::eFragment, *fragment_shader, "main"));
        shader_stage_cis.emplace_back(
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertex_shader, "main"));

        auto binding_description = Vertex::get_binding_description();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();

        vk::PipelineVertexInputStateCreateInfo vertex_input_ci{};
        vertex_input_ci.setVertexBindingDescriptions(binding_description);
        vertex_input_ci.setVertexAttributeDescriptions(attribute_descriptions);

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_ci{};
        input_assembly_ci.setTopology(vk::PrimitiveTopology::eTriangleList);
        input_assembly_ci.setPrimitiveRestartEnable(VK_FALSE);

        auto size = get_window_size<float>();
        vk::Viewport viewport{ 0.0F, 0.0F, size.first, size.second, 0.0F, 1.0F };
        vk::Rect2D scissor{ {}, m_extent };

        constexpr std::array dynamic_states{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state_ci({}, dynamic_states);

        vk::PipelineViewportStateCreateInfo viewport_ci{};
        viewport_ci.setViewportCount(1);
        viewport_ci.setScissorCount(1);

        vk::PipelineRasterizationStateCreateInfo rasterizer_ci{};
        rasterizer_ci.setDepthClampEnable(VK_FALSE);
        rasterizer_ci.setRasterizerDiscardEnable(VK_FALSE);
        rasterizer_ci.setPolygonMode(vk::PolygonMode::eFill);
        rasterizer_ci.setLineWidth(1.0F);
        rasterizer_ci.setCullMode(vk::CullModeFlagBits::eBack);
        rasterizer_ci.setFrontFace(vk::FrontFace::eClockwise);
        rasterizer_ci.setDepthBiasEnable(VK_FALSE);

        vk::PipelineMultisampleStateCreateInfo multisampling_ci{};
        multisampling_ci.setSampleShadingEnable(VK_TRUE);
        multisampling_ci.setRasterizationSamples(m_msaa_samples);
        multisampling_ci.setMinSampleShading(0.2F);

        vk::PipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);
        color_blend_attachment.setBlendEnable(VK_FALSE);
        // color_blend_attachment.setBlendEnable(VK_TRUE);
        // color_blend_attachment.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
        // color_blend_attachment.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
        // color_blend_attachment.setColorBlendOp(vk::BlendOp::eAdd);
        // color_blend_attachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        // color_blend_attachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        // color_blend_attachment.setAlphaBlendOp(vk::BlendOp::eAdd);

        vk::PipelineColorBlendStateCreateInfo color_blend_ci{};
        color_blend_ci.setLogicOpEnable(VK_FALSE);
        color_blend_ci.setLogicOp(vk::LogicOp::eCopy);
        color_blend_ci.setAttachments(color_blend_attachment);

        // vk::PipelineDepthStencilStateCreateInfo depth_stencil_ci{};
        // depth_stencil_ci.setDepthTestEnable(VK_TRUE);
        // depth_stencil_ci.setDepthWriteEnable(VK_TRUE);
        // depth_stencil_ci.setDepthCompareOp(vk::CompareOp::eLess);
        // depth_stencil_ci.setDepthBoundsTestEnable(VK_FALSE);
        // depth_stencil_ci.setStencilTestEnable(VK_FALSE);

        vk::GraphicsPipelineCreateInfo graphics_pipeline_ci{};
        graphics_pipeline_ci.setStages(shader_stage_cis);
        graphics_pipeline_ci.setPVertexInputState(&vertex_input_ci);
        graphics_pipeline_ci.setPInputAssemblyState(&input_assembly_ci);
        graphics_pipeline_ci.setPViewportState(&viewport_ci);
        graphics_pipeline_ci.setPRasterizationState(&rasterizer_ci);
        graphics_pipeline_ci.setPMultisampleState(&multisampling_ci);
        // graphics_pipeline_ci.setPDepthStencilState(&depth_stencil_ci);
        graphics_pipeline_ci.setPColorBlendState(&color_blend_ci);
        graphics_pipeline_ci.setPDynamicState(&dynamic_state_ci);
        graphics_pipeline_ci.setLayout(*m_pipeline_layout);
        graphics_pipeline_ci.setRenderPass(*m_render_pass);
        graphics_pipeline_ci.setSubpass(0);

        auto create = vulkan->get_logical_device().createGraphicsPipelineUnique({}, graphics_pipeline_ci);

        if (create.result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        return std::move(create.value);
    }

    vk::UniqueSwapchainKHR Window::create_swapchain(const vk::SwapchainKHR& old_swapchain) const {
        uint32_t image_count = m_support.capabilities.minImageCount + 1;
        if (m_support.capabilities.maxImageCount > 0 && image_count > m_support.capabilities.maxImageCount) {
            image_count = m_support.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR ci{};
        ci.setSurface(*m_surface);
        ci.setMinImageCount(image_count);
        ci.setImageFormat(m_surface_format.format);
        ci.setImageColorSpace(m_surface_format.colorSpace);
        ci.setImageExtent(m_extent);
        ci.setImageArrayLayers(1);
        ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

        auto indices = QueueFamilyIndices(vulkan->get_physical_device(), *m_surface);
        auto index_values = indices.values();
        if (indices.set().size() > 1) {
            ci.setImageSharingMode(vk::SharingMode::eConcurrent);
            ci.setQueueFamilyIndices(index_values);
        } else {
            ci.setImageSharingMode(vk::SharingMode::eExclusive);
        }

        ci.setPreTransform(m_support.capabilities.currentTransform);
        ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
        ci.setPresentMode(m_present_mode);
        ci.setClipped(VK_TRUE);
        ci.setOldSwapchain(old_swapchain);

        return vulkan->get_logical_device().createSwapchainKHRUnique(ci);
    }

    std::vector<vk::UniqueImageView> Window::create_swapchain_views() const {
        std::vector<vk::UniqueImageView> views;

        for (auto image : m_swapchain_images) {
            vk::ImageViewCreateInfo ci{};
            ci.setImage(image);
            ci.setViewType(vk::ImageViewType::e2D);
            ci.setFormat(m_surface_format.format);
            ci.setComponents({ {}, {}, {}, {} });
            ci.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            views.emplace_back(vulkan->get_logical_device().createImageViewUnique(ci));
        }

        return std::move(views);
    }

    std::unique_ptr<ImageResource> Window::create_color_image() const {
        ImageProperties properties{ get_window_size<uint32_t>(),
                                    1,
                                    m_msaa_samples,
                                    m_surface_format.format,
                                    vk::ImageTiling::eOptimal,
                                    vk::ImageAspectFlagBits::eColor,
                                    vk::ImageUsageFlagBits::eTransientAttachment |
                                        vk::ImageUsageFlagBits::eColorAttachment,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal };
        return std::make_unique<ImageResource>(vulkan, properties);
    }

    std::unique_ptr<ImageResource> Window::create_depth_image() const {
        ImageProperties properties{ get_window_size<uint32_t>(),
                                    1,
                                    m_msaa_samples,
                                    vulkan->find_depth_format(),
                                    vk::ImageTiling::eOptimal,
                                    vk::ImageAspectFlagBits::eDepth,
                                    vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal };
        auto image = std::make_unique<ImageResource>(vulkan, properties);
        image->transition_layout(m_graphics_queue, vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eDepthStencilAttachmentOptimal);
        return std::move(image);
    }

    std::vector<vk::UniqueFramebuffer> Window::create_framebuffers() const {
        std::vector<vk::UniqueFramebuffer> framebuffers;

        for (const auto& image_view : m_swapchain_views) {
            // const std::array attachments{ *m_color_image->view, *m_depth_image->view, *image_view };
            vk::FramebufferCreateInfo ci{};
            ci.setRenderPass(*m_render_pass);
            ci.setAttachments(*image_view);
            ci.setWidth(m_extent.width);
            ci.setHeight(m_extent.height);
            ci.setLayers(1);
            framebuffers.push_back(vulkan->get_logical_device().createFramebufferUnique(ci));
        }

        return std::move(framebuffers);
    }

    void Window::get_swapchain_details() {
        m_support = { vulkan->get_physical_device(), *m_surface };

        for (auto format : m_support.formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                m_surface_format = format;
            }
        }

        for (auto mode : m_support.present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                m_present_mode = mode;
            }
        }

        if (m_support.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            m_extent = m_support.capabilities.currentExtent;
        }
    }

    void Window::record_command_buffer(uint32_t index) {
        const auto& frame = m_frames->current();

        vk::CommandBufferBeginInfo command_buffer_bi{};
        command_buffer_bi.setFlags({});
        command_buffer_bi.setPInheritanceInfo({});
        frame.command_buffer->begin(command_buffer_bi);

        constexpr std::array clear_color{ 0.0F, 0.0F, 0.0F, 1.0F };
        const vk::ClearValue clear_value{ clear_color };
        const std::array clear_values{ clear_value, clear_value };
        auto size = get_window_size<float>();
        vk::Viewport viewport{ 0.0F, 0.0F, size.first, size.second, 0.0F, 1.0F };
        vk::Rect2D scissor{ { 0, 0 }, m_extent };

        vk::RenderPassBeginInfo render_pass_bi{};
        render_pass_bi.setRenderPass(*m_render_pass);
        render_pass_bi.setFramebuffer(*m_framebuffers.at(index));
        render_pass_bi.setRenderArea({ { 0, 0 }, m_extent });
        render_pass_bi.setClearValues(clear_values);

        frame.command_buffer->beginRenderPass(render_pass_bi, vk::SubpassContents::eInline);
        frame.command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphics_pipeline);
        frame.command_buffer->setViewport(0, viewport);
        frame.command_buffer->setScissor(0, scissor);

        for (const auto& object : m_objects) {
            constexpr std::array no_offset{ vk::DeviceSize(0) };
            frame.command_buffer->bindVertexBuffers(0, *object.vertex_buffer, no_offset);
            frame.command_buffer->bindIndexBuffer(*object.index_buffer, 0,
                                                  vk::IndexType::eUint16);
            frame.command_buffer->drawIndexed(object.index_count, 1, 0, 0, 0);
        }

        frame.command_buffer->endRenderPass();
        frame.command_buffer->end();
    }
}
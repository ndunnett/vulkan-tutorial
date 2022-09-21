#include "window.h"

namespace tutorial {
    Swapchain::Swapchain(Window* window) : parent(window) {
        create_object();
        create_image_views();
    }

    void Swapchain::recreate() {
        parent->vulkan->get_logical_device().waitIdle();
        m_image_views.clear();
        create_object(m_object.release());
        create_image_views();
    }

    void Swapchain::create_object(const vk::SwapchainKHR& old_swapchain) {
        parent->m_support = { parent->vulkan->get_physical_device(), *parent->m_surface };

        for (auto format : parent->m_support.formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                parent->m_surface_format = format;
            }
        }

        for (auto mode : parent->m_support.present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                parent->m_present_mode = mode;
            }
        }

        if (parent->m_support.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            parent->m_extent = parent->m_support.capabilities.currentExtent;
        }

        uint32_t image_count = parent->m_support.capabilities.minImageCount + 1;
        if (parent->m_support.capabilities.maxImageCount > 0 &&
            image_count > parent->m_support.capabilities.maxImageCount) {
            image_count = parent->m_support.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR ci{};
        ci.setSurface(*parent->m_surface);
        ci.setMinImageCount(image_count);
        ci.setImageFormat(parent->m_surface_format.format);
        ci.setImageColorSpace(parent->m_surface_format.colorSpace);
        ci.setImageExtent(parent->m_extent);
        ci.setImageArrayLayers(1);
        ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

        auto indices = QueueFamilyIndices(parent->vulkan->get_physical_device(), *parent->m_surface);
        auto index_values = indices.values();
        if (indices.set().size() > 1) {
            ci.setImageSharingMode(vk::SharingMode::eConcurrent);
            ci.setQueueFamilyIndices(index_values);
        } else {
            ci.setImageSharingMode(vk::SharingMode::eExclusive);
        }

        ci.setPreTransform(parent->m_support.capabilities.currentTransform);
        ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
        ci.setPresentMode(parent->m_present_mode);
        ci.setClipped(VK_TRUE);
        ci.setOldSwapchain(old_swapchain);

        m_object = parent->vulkan->get_logical_device().createSwapchainKHRUnique(ci);
    }

    void Swapchain::create_image_views() {
        m_images = parent->vulkan->get_logical_device().getSwapchainImagesKHR(*m_object);

        for (auto image : m_images) {
            vk::ImageViewCreateInfo ci{};
            ci.setImage(image);
            ci.setViewType(vk::ImageViewType::e2D);
            ci.setFormat(parent->m_surface_format.format);
            ci.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            m_image_views.emplace_back(parent->vulkan->get_logical_device().createImageViewUnique(ci));
        }
    }

    vk::UniqueShaderModule
    GraphicsPipeline::create_shader_module(std::pair<shaderc_shader_kind, std::string_view> shader_source) {
        auto compiled_shader = compile_shader(shader_source);
        return parent->vulkan->get_logical_device().createShaderModuleUnique({ {}, compiled_shader });
    }

    vk::UniqueRenderPass GraphicsPipeline::create_render_pass() {
        vk::AttachmentDescription color_attachment{};
        color_attachment.setFormat(parent->m_surface_format.format);
        color_attachment.setSamples(parent->m_msaa_samples);
        color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
        color_attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::AttachmentReference color_attachment_ref{};
        color_attachment_ref.setAttachment(0);
        color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::AttachmentDescription depth_attachment{};
        depth_attachment.setFormat(parent->vulkan->find_depth_format());
        depth_attachment.setSamples(parent->m_msaa_samples);
        depth_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        depth_attachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
        depth_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        depth_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        depth_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
        depth_attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentReference depth_attachment_ref{};
        depth_attachment_ref.setAttachment(1);
        depth_attachment_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentDescription color_attachment_resolve{};
        color_attachment_resolve.setFormat(parent->m_surface_format.format);
        color_attachment_resolve.setSamples(vk::SampleCountFlagBits::e1);
        color_attachment_resolve.setLoadOp(vk::AttachmentLoadOp::eDontCare);
        color_attachment_resolve.setStoreOp(vk::AttachmentStoreOp::eStore);
        color_attachment_resolve.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        color_attachment_resolve.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        color_attachment_resolve.setInitialLayout(vk::ImageLayout::eUndefined);
        color_attachment_resolve.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference color_attachment_resolve_ref{};
        color_attachment_resolve_ref.setAttachment(2);
        color_attachment_resolve_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass{};
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subpass.setColorAttachments(color_attachment_ref);
        subpass.setPDepthStencilAttachment(&depth_attachment_ref);
        subpass.setResolveAttachments(color_attachment_resolve_ref);

        vk::SubpassDependency dependency{};
        dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
        dependency.setDstSubpass(0);
        dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests);
        dependency.setSrcAccessMask(vk::AccessFlagBits::eNone);
        dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests);
        dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                    vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        const std::array attachments{ color_attachment, depth_attachment, color_attachment_resolve };

        vk::RenderPassCreateInfo render_pass_ci{};
        render_pass_ci.setAttachments(attachments);
        render_pass_ci.setSubpasses(subpass);
        render_pass_ci.setDependencies(dependency);

        return parent->vulkan->get_logical_device().createRenderPassUnique(render_pass_ci);
    }

    vk::UniquePipelineLayout GraphicsPipeline::create_pipeline_layout() {
        vk::PipelineLayoutCreateInfo pipeline_layout_ci{};

        return parent->vulkan->get_logical_device().createPipelineLayoutUnique(pipeline_layout_ci);
    }

    vk::UniquePipeline GraphicsPipeline::create_graphics_pipeline() {
        auto fragment_shader = create_shader_module(fragment_shader_source);
        auto vertex_shader = create_shader_module(vertex_shader_source);

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_cis;
        shader_stage_cis.emplace_back(vk::PipelineShaderStageCreateInfo(
            {}, vk::ShaderStageFlagBits::eFragment, *fragment_shader, "main"));
        shader_stage_cis.emplace_back(
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertex_shader, "main"));

        vk::PipelineVertexInputStateCreateInfo vertex_input_ci{};

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_ci{};
        input_assembly_ci.setTopology(vk::PrimitiveTopology::eTriangleList);
        input_assembly_ci.setPrimitiveRestartEnable(VK_FALSE);

        auto size = parent->get_window_size<float>();
        vk::Viewport viewport{ 0.0F, 0.0F, size.first, size.second, 0.0F, 1.0F };
        vk::Rect2D scissor{ {}, parent->m_extent };

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
        multisampling_ci.setRasterizationSamples(parent->m_msaa_samples);
        multisampling_ci.setMinSampleShading(0.2F);

        vk::PipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);
        color_blend_attachment.setBlendEnable(VK_TRUE);
        color_blend_attachment.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
        color_blend_attachment.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
        color_blend_attachment.setColorBlendOp(vk::BlendOp::eAdd);
        color_blend_attachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        color_blend_attachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        color_blend_attachment.setAlphaBlendOp(vk::BlendOp::eAdd);
        const std::array color_blend_attachments{ color_blend_attachment };

        vk::PipelineColorBlendStateCreateInfo color_blend_ci{};
        color_blend_ci.setLogicOpEnable(VK_FALSE);
        color_blend_ci.setAttachments(color_blend_attachments);

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_ci{};
        depth_stencil_ci.setDepthTestEnable(VK_TRUE);
        depth_stencil_ci.setDepthWriteEnable(VK_TRUE);
        depth_stencil_ci.setDepthCompareOp(vk::CompareOp::eLess);
        depth_stencil_ci.setDepthBoundsTestEnable(VK_FALSE);
        depth_stencil_ci.setStencilTestEnable(VK_FALSE);

        vk::GraphicsPipelineCreateInfo graphics_pipeline_ci{};
        graphics_pipeline_ci.setStages(shader_stage_cis);
        graphics_pipeline_ci.setPVertexInputState(&vertex_input_ci);
        graphics_pipeline_ci.setPInputAssemblyState(&input_assembly_ci);
        graphics_pipeline_ci.setPViewportState(&viewport_ci);
        graphics_pipeline_ci.setPRasterizationState(&rasterizer_ci);
        graphics_pipeline_ci.setPMultisampleState(&multisampling_ci);
        graphics_pipeline_ci.setPDepthStencilState(&depth_stencil_ci);
        graphics_pipeline_ci.setPColorBlendState(&color_blend_ci);
        graphics_pipeline_ci.setPDynamicState(&dynamic_state_ci);
        graphics_pipeline_ci.setLayout(*m_pipeline_layout);
        graphics_pipeline_ci.setRenderPass(*m_render_pass);
        graphics_pipeline_ci.setSubpass(0);

        auto graphics_pipeline =
            parent->vulkan->get_logical_device().createGraphicsPipelineUnique({}, graphics_pipeline_ci);

        if (graphics_pipeline.result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        return std::move(graphics_pipeline.value);
    }

    Window::Window(VulkanCore* vulkan, std::string_view title, std::pair<int, int> size,
                   const std::vector<std::pair<int, int>>& hints)
        : vulkan(vulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        for (auto hint : hints) {
            glfwWindowHint(hint.first, hint.second);
        }

        m_window = glfwCreateWindow(size.first, size.second, title.data(), nullptr, nullptr);
        set_extent(get_window_size<uint32_t>());

        if (!m_window) {
            throw std::runtime_error("failed to create GLFW window!");
        }

        VkSurfaceKHR raw_surface{};
        if (glfwCreateWindowSurface(vulkan->get_instance(), m_window, nullptr, &raw_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        vulkan->initialise_devices(raw_surface);
        m_msaa_samples = vulkan->get_max_msaa_samples();
        std::tie(m_graphics_queue, m_present_queue) = vulkan->get_queues();
        m_surface = vk::UniqueSurfaceKHR(raw_surface, vulkan->get_deleter());
        m_swapchain = std::make_unique<Swapchain>(this);
        m_pipeline = std::make_unique<GraphicsPipeline>(this);
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
}
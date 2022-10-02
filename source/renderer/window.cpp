#include "window.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tutorial {
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

        const std::vector<Vertex> vertices{ { { -0.5F, -0.5F }, { 1.0F, 0.0F, 0.0F }, { 1.0F, 0.0F } },
                                            { { 0.5F, -0.5F }, { 0.0F, 1.0F, 0.0F }, { 0.0F, 0.0F } },
                                            { { 0.5F, 0.5F }, { 0.0F, 0.0F, 1.0F }, { 0.0F, 1.0F } },
                                            { { -0.5F, 0.5F }, { 1.0F, 1.0F, 1.0F }, { 1.0F, 1.0F } } };

        const std::vector<uint16_t> indices{ 0, 1, 2, 2, 3, 0 };

        m_surface = create_surface();
        vulkan->initialise_devices(*m_surface);
        // m_msaa_samples = vulkan->get_max_msaa_samples();
        m_graphics_queue = vulkan->logical_device->getQueue(vulkan->queue_family_indices.graphics.value(), 0);
        m_present_queue = vulkan->logical_device->getQueue(vulkan->queue_family_indices.present.value(), 0);
        get_swapchain_details();
        m_swapchain = create_swapchain();
        m_swapchain_images = vulkan->logical_device->getSwapchainImagesKHR(*m_swapchain);
        m_swapchain_views = create_swapchain_views();
        m_render_pass = create_render_pass();
        m_descriptor_set_layout = create_descriptor_set_layout();
        m_pipeline_layout = create_pipeline_layout();
        m_graphics_pipeline = create_graphics_pipeline();
        // m_color_image = create_color_image();
        // m_depth_image = create_depth_image();
        m_framebuffers = create_framebuffers();
        m_sampler = create_sampler();
        m_frames = std::make_unique<FrameTransients>(vulkan);

        m_texture = create_texture("textures/texture.jpg");
        m_object = std::make_unique<Object>(vulkan, m_graphics_queue, vertices, indices);

        m_descriptor_pool = create_descriptor_pool();
        m_descriptor_sets = create_descriptor_sets();
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
    }

    void Window::rebuild_swapchain() {
        update_size();
        vulkan->logical_device->waitIdle();
        m_framebuffers.clear();
        m_swapchain_views.clear();
        m_swapchain_images.clear();
        get_swapchain_details();
        m_swapchain = create_swapchain(*m_swapchain);
        m_swapchain_images = vulkan->logical_device->getSwapchainImagesKHR(*m_swapchain);
        m_swapchain_views = create_swapchain_views();
        // m_color_image = create_color_image();
        // m_depth_image = create_depth_image();
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

        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float time =
            std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
        auto size = get_size<float>();

        UniformBufferObject new_ubo{};
        new_ubo.model = glm::rotate(glm::mat4(1.0F), time * glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
        new_ubo.view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F),
                                   glm::vec3(0.0F, 0.0F, 1.0F));
        new_ubo.proj = glm::perspective(glm::radians(45.0F), size.first / size.second, 0.1F, 10.0F);
        new_ubo.proj[1][1] *= -1;
        m_frames->update_ubo(new_ubo);
        m_frames->submit(m_graphics_queue, vk::PipelineStageFlagBits::eColorAttachmentOutput);

        auto present = m_frames->present(m_present_queue, *m_swapchain, index.value);

        if (present == vk::Result::eErrorOutOfDateKHR || present == vk::Result::eSuboptimalKHR ||
            m_framebuffer_resized) {
            m_framebuffer_resized = false;
            rebuild_swapchain();
        }

        m_frames->next_frame();
    }

    std::unique_ptr<ImageResource> Window::create_texture(std::string_view path) {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        size_t memory_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        auto [staging_buffer, staging_memory] = vulkan->create_buffer(
            memory_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*staging_memory, pixels, memory_size);
        stbi_image_free(pixels);

        std::pair<uint32_t, uint32_t> image_size{ width, height };
        ImageProperties properties{ image_size,
                                    1,
                                    vk::SampleCountFlagBits::e1,
                                    vk::Format::eR8G8B8A8Srgb,
                                    vk::ImageTiling::eOptimal,
                                    vk::ImageAspectFlagBits::eColor,
                                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal };
        auto image = std::make_unique<ImageResource>(vulkan, properties);

        image->transition_layout(m_graphics_queue, vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eTransferDstOptimal);
        image->copy_buffer(m_graphics_queue, *staging_buffer, image_size);
        image->transition_layout(m_graphics_queue, vk::ImageLayout::eTransferDstOptimal,
                                 vk::ImageLayout::eShaderReadOnlyOptimal);

        return std::move(image);
    }

    vk::UniqueSurfaceKHR Window::create_surface() const {
        VkSurfaceKHR raw_surface{};

        if (glfwCreateWindowSurface(vulkan->instance.get(), m_window, nullptr, &raw_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        return vk::UniqueSurfaceKHR(raw_surface, vulkan->get_deleter());
    }

    vk::UniqueShaderModule Window::create_shader_module(const ShaderSource& shader_source) const {
        auto compiled_shader = compile_shader(shader_source);
        return vulkan->logical_device->createShaderModuleUnique({ {}, compiled_shader });
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

        return vulkan->logical_device->createRenderPassUnique(render_pass_ci);
    }

    vk::UniqueDescriptorSetLayout Window::create_descriptor_set_layout() const {
        vk::DescriptorSetLayoutBinding ubo_binding{};
        ubo_binding.setBinding(0);
        ubo_binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        ubo_binding.setDescriptorCount(1);
        ubo_binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

        vk::DescriptorSetLayoutBinding sampler_binding{};
        sampler_binding.setBinding(1);
        sampler_binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        sampler_binding.setDescriptorCount(1);
        sampler_binding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        const std::array bindings{ ubo_binding, sampler_binding };

        vk::DescriptorSetLayoutCreateInfo ci{};
        ci.setBindings(bindings);

        return vulkan->logical_device->createDescriptorSetLayoutUnique(ci);
    }

    vk::UniquePipelineLayout Window::create_pipeline_layout() const {
        vk::PipelineLayoutCreateInfo ci{};
        ci.setSetLayouts(*m_descriptor_set_layout);
        return vulkan->logical_device->createPipelineLayoutUnique(ci);
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

        auto size = get_size<float>();
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
        rasterizer_ci.setFrontFace(vk::FrontFace::eCounterClockwise);
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

        auto create = vulkan->logical_device->createGraphicsPipelineUnique({}, graphics_pipeline_ci);

        if (create.result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        return std::move(create.value);
    }

    vk::UniqueDescriptorPool Window::create_descriptor_pool() const {
        vk::DescriptorPoolSize ubo_pool_size{};
        ubo_pool_size.setType(vk::DescriptorType::eUniformBuffer);
        ubo_pool_size.setDescriptorCount(m_frames->size());

        vk::DescriptorPoolSize sampler_pool_size{};
        sampler_pool_size.setType(vk::DescriptorType::eCombinedImageSampler);
        sampler_pool_size.setDescriptorCount(m_frames->size());

        const std::array pool_sizes{ ubo_pool_size, sampler_pool_size };

        vk::DescriptorPoolCreateInfo ci{};
        ci.setPoolSizes(pool_sizes);
        ci.setMaxSets(m_frames->size());
        ci.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

        return vulkan->logical_device->createDescriptorPoolUnique(ci);
    }

    std::vector<vk::UniqueDescriptorSet> Window::create_descriptor_sets() const {
        std::vector<vk::DescriptorSetLayout> layouts(m_frames->size(), *m_descriptor_set_layout);

        vk::DescriptorSetAllocateInfo ai{};
        ai.setDescriptorPool(*m_descriptor_pool);
        ai.setSetLayouts(layouts);

        std::vector<vk::UniqueDescriptorSet> descriptor_sets =
            vulkan->logical_device->allocateDescriptorSetsUnique(ai);

        for (size_t i = 0; i < m_frames->size(); i++) {
            vk::DescriptorBufferInfo buffer_info{};
            buffer_info.setBuffer(m_frames->get_ubo_buffer(i));
            buffer_info.setOffset(0);
            buffer_info.setRange(sizeof(UniformBufferObject));

            vk::WriteDescriptorSet ubo_write{};
            ubo_write.setDstSet(*descriptor_sets.at(i));
            ubo_write.setDstBinding(0);
            ubo_write.setDstArrayElement(0);
            ubo_write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
            ubo_write.setDescriptorCount(1);
            ubo_write.setBufferInfo(buffer_info);

            vk::DescriptorImageInfo image_info{};
            image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            image_info.setImageView(*m_texture->view);
            image_info.setSampler(*m_sampler);

            vk::WriteDescriptorSet sampler_write{};
            sampler_write.setDstSet(*descriptor_sets.at(i));
            sampler_write.setDstBinding(1);
            sampler_write.setDstArrayElement(0);
            sampler_write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            sampler_write.setDescriptorCount(1);
            sampler_write.setImageInfo(image_info);

            const std::array writes{ ubo_write, sampler_write };
            vulkan->logical_device->updateDescriptorSets(writes, nullptr);
        }

        return std::move(descriptor_sets);
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

        auto indices = QueueFamilyIndices(vulkan->physical_device, *m_surface);
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

        return vulkan->logical_device->createSwapchainKHRUnique(ci);
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
            views.emplace_back(vulkan->logical_device->createImageViewUnique(ci));
        }

        return std::move(views);
    }

    // std::unique_ptr<ImageResource> Window::create_color_image() const {
    //     ImageProperties properties{ get_size<uint32_t>(),
    //                                 1,
    //                                 m_msaa_samples,
    //                                 m_surface_format.format,
    //                                 vk::ImageTiling::eOptimal,
    //                                 vk::ImageAspectFlagBits::eColor,
    //                                 vk::ImageUsageFlagBits::eTransientAttachment |
    //                                     vk::ImageUsageFlagBits::eColorAttachment,
    //                                 vk::MemoryPropertyFlagBits::eDeviceLocal };
    //     return std::make_unique<ImageResource>(vulkan, properties);
    // }

    // std::unique_ptr<ImageResource> Window::create_depth_image() const {
    //     ImageProperties properties{ get_size<uint32_t>(),
    //                                 1,
    //                                 m_msaa_samples,
    //                                 vulkan->find_depth_format(),
    //                                 vk::ImageTiling::eOptimal,
    //                                 vk::ImageAspectFlagBits::eDepth,
    //                                 vk::ImageUsageFlagBits::eDepthStencilAttachment,
    //                                 vk::MemoryPropertyFlagBits::eDeviceLocal };
    //     auto image = std::make_unique<ImageResource>(vulkan, properties);
    //     image->transition_layout(m_graphics_queue, vk::ImageLayout::eUndefined,
    //                              vk::ImageLayout::eDepthStencilAttachmentOptimal);
    //     return std::move(image);
    // }

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
            framebuffers.push_back(vulkan->logical_device->createFramebufferUnique(ci));
        }

        return std::move(framebuffers);
    }

    vk::UniqueSampler Window::create_sampler() const {
        auto properties = vulkan->physical_device.getProperties();

        vk::SamplerCreateInfo ci{};
        ci.setMagFilter(vk::Filter::eLinear);
        ci.setMinFilter(vk::Filter::eLinear);
        ci.setAddressModeU(vk::SamplerAddressMode::eRepeat);
        ci.setAddressModeV(vk::SamplerAddressMode::eRepeat);
        ci.setAddressModeW(vk::SamplerAddressMode::eRepeat);
        ci.setAnisotropyEnable(VK_TRUE);
        ci.setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);
        ci.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
        ci.setUnnormalizedCoordinates(VK_FALSE);
        ci.setCompareEnable(VK_FALSE);
        ci.setCompareOp(vk::CompareOp::eAlways);
        ci.setMipmapMode(vk::SamplerMipmapMode::eLinear);
        ci.setMipLodBias(0.0F);
        ci.setMinLod(0.0F);
        ci.setMaxLod(static_cast<float>(1));

        return vulkan->logical_device->createSamplerUnique(ci);
    }

    void Window::update_size() {
        int width = 0;
        int height = 0;

        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_extent.width = std::clamp(static_cast<uint32_t>(width), m_support.capabilities.minImageExtent.width,
                                    m_support.capabilities.maxImageExtent.width);
        m_extent.height =
            std::clamp(static_cast<uint32_t>(height), m_support.capabilities.minImageExtent.height,
                       m_support.capabilities.maxImageExtent.height);
    }

    void Window::get_swapchain_details() {
        m_support = { vulkan->physical_device, *m_surface };

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

        auto size = get_size<float>();
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

        // for (const auto& object : m_objects) {
        //     constexpr std::array no_offset{ vk::DeviceSize(0) };
        //     frame.command_buffer->bindVertexBuffers(0, *object.vertex_buffer, no_offset);
        //     frame.command_buffer->bindIndexBuffer(*object.index_buffer, 0, vk::IndexType::eUint16);
        //     frame.command_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipeline_layout,
        //     0,
        //                                              *m_descriptor_sets.at(m_frames->current_index()),
        //                                              nullptr);
        //     frame.command_buffer->drawIndexed(object.index_count, 1, 0, 0, 0);
        // }

        constexpr std::array no_offset{ vk::DeviceSize(0) };
        frame.command_buffer->bindVertexBuffers(0, *m_object->vertex_buffer, no_offset);
        frame.command_buffer->bindIndexBuffer(*m_object->index_buffer, 0, vk::IndexType::eUint16);
        frame.command_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipeline_layout, 0,
                                                 *m_descriptor_sets.at(m_frames->current_index()), nullptr);
        frame.command_buffer->drawIndexed(m_object->index_count, 1, 0, 0, 0);

        frame.command_buffer->endRenderPass();
        frame.command_buffer->end();
    }
}
// Runs on the existing GPU worker, before the first guest Composite. Reuse its
// swapchain and composition pass without guest captures or FPS notifications.
void RendererVulkan::PresentLoading() {
    // The Vulkan scheduler belongs to the GPU split's back once it runs (Switch 2 titles at the
    // Fast GPU level). Recording from this thread as well crashed in Scheduler::Record.
    VideoCommon::GpuSplit::Get().Run([this] {
        Frame* frame = present_manager.GetRenderFrame();
        scheduler.RequestOutsideRenderPassOperationContext();
        blit_swapchain.DrawToFrame(device, rasterizer, frame, {},
                                  render_window.GetFramebufferLayout(), swapchain.GetImageCount(),
                                  swapchain.GetImageViewFormat(), true);
        scheduler.Flush(*frame->render_ready);
        present_manager.Present(frame);
    });
}

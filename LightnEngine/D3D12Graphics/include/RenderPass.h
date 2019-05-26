#pragma once

struct RenderSettings;

struct IRenderPass {
	virtual void setupRenderCommand(RenderSettings& settings) = 0;
	virtual void destroy() = 0;
};
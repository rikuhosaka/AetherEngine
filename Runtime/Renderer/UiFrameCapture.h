#pragma once

// ImGui の入力キャプチャ結果。実体は RenderSystem が作り、UI サブシステムが読む。
struct UiFrameCapture
{
	bool wantCaptureMouse = false;
	bool wantCaptureKeyboard = false;
	bool anyWindowHovered = false;
};

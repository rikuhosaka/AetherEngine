#include "Engine/Platform/Window.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Command/DX12SwapChain.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12DSVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12RTVAllocator.h"
#include "Engine/RHI/DX12/Memory/DX12Buffer.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Resource/DX12PixelShader.h"
#include "Engine/RHI/DX12/Resource/DX12VertexShader.h"
#include "Engine/RHI/DX12/Pipeline/DX12PipelineState.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Sync/DX12Fence.h"
#include "Engine/Renderer/Core/Renderer.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// �E�B���h�E�̍쐬
	g_hwnd = CreateGameWindow(hInstance);
	if (!g_hwnd)
		return -1;

	// DirectX 12�f�o�C�X�̏�����
	std::unique_ptr<RHIDevice> device = std::make_unique<DX12Device>();
	device->Initialize();
	
	std::unique_ptr<RHICommandList> commandList = device->CreateCommandList();
	std::unique_ptr<RHICommandQueue> commandQueue = device->CreateCommandQueue();

	std::unique_ptr<RHISwapChain> swapChain = device->CreateSwapChain(g_hwnd, 1920, 1080, commandQueue.get());


	

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		MSGProcess(msg);
	}
	return 0;
}
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderReflectionConvert.h"

#include <d3d12shader.h>

namespace
{
	[[nodiscard]] ShaderStageFlags ToShaderStageFlags(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex:
			return ShaderStageFlags::Vertex;
		case ShaderStage::Pixel:
			return ShaderStageFlags::Pixel;
		case ShaderStage::Compute:
			return ShaderStageFlags::Compute;
		case ShaderStage::Geometry:
			return ShaderStageFlags::Geometry;
		case ShaderStage::Hull:
			return ShaderStageFlags::Hull;
		case ShaderStage::Domain:
			return ShaderStageFlags::Domain;
		default:
			return ShaderStageFlags::None;
		}
	}

	[[nodiscard]] ShaderResourceAccess ToShaderResourceAccess(const D3D12_SHADER_INPUT_BIND_DESC& bindDesc)
	{
		switch (bindDesc.Type)
		{
		case D3D_SIT_UAV_RWTYPED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
		case D3D_SIT_UAV_APPEND_STRUCTURED:
		case D3D_SIT_UAV_CONSUME_STRUCTURED:
		case D3D_SIT_UAV_RWBYTEADDRESS:
			return ShaderResourceAccess::ReadWrite;
		default:
			return ShaderResourceAccess::ReadOnly;
		}
	}

	[[nodiscard]] ShaderResourceType ToShaderResourceType(const D3D12_SHADER_INPUT_BIND_DESC& bindDesc)
	{
		switch (bindDesc.Type)
		{
		case D3D_SIT_CBUFFER:
			return ShaderResourceType::ConstantBuffer;

		case D3D_SIT_SAMPLER:
			return ShaderResourceType::Sampler;

		case D3D_SIT_TEXTURE:
		{
			switch (bindDesc.Dimension)
			{
			case D3D_SRV_DIMENSION_TEXTURE1D:
				return ShaderResourceType::Texture1D;
			case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
				return ShaderResourceType::Texture1DArray;
			case D3D_SRV_DIMENSION_TEXTURE2D:
				return ShaderResourceType::Texture2D;
			case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
				return ShaderResourceType::Texture2DArray;
			case D3D_SRV_DIMENSION_TEXTURE2DMS:
				return ShaderResourceType::Texture2DMS;
			case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
				return ShaderResourceType::Texture2DMSArray;
			case D3D_SRV_DIMENSION_TEXTURE3D:
				return ShaderResourceType::Texture3D;
			case D3D_SRV_DIMENSION_TEXTURECUBE:
				return ShaderResourceType::TextureCube;
			case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
				return ShaderResourceType::TextureCubeArray;
			default:
				return ShaderResourceType::Unknown;
			}
		}

		case D3D_SIT_STRUCTURED:
			return ShaderResourceType::StructuredBuffer;

		case D3D_SIT_BYTEADDRESS:
			return ShaderResourceType::ByteAddressBuffer;

		case D3D_SIT_UAV_RWTYPED:
		{
			switch (bindDesc.Dimension)
			{
			case D3D_SRV_DIMENSION_TEXTURE1D:
				return ShaderResourceType::RWTexture1D;
			case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
				return ShaderResourceType::RWTexture1DArray;
			case D3D_SRV_DIMENSION_TEXTURE2D:
				return ShaderResourceType::RWTexture2D;
			case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
				return ShaderResourceType::RWTexture2DArray;
			case D3D_SRV_DIMENSION_TEXTURE3D:
				return ShaderResourceType::RWTexture3D;
			default:
				return ShaderResourceType::Unknown;
			}
		}

		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
		case D3D_SIT_UAV_APPEND_STRUCTURED:
		case D3D_SIT_UAV_CONSUME_STRUCTURED:
			return ShaderResourceType::RWStructuredBuffer;

		case D3D_SIT_UAV_RWBYTEADDRESS:
			return ShaderResourceType::RWByteAddressBuffer;

		case D3D_SIT_RTACCELERATIONSTRUCTURE:
			return ShaderResourceType::AccelerationStructure;

		case D3D_SIT_TBUFFER:
		default:
			return ShaderResourceType::Unknown;
		}
	}

	[[nodiscard]] ShaderVariableClass ToShaderVariableClass(const D3D12_SHADER_TYPE_DESC& typeDesc)
	{
		if (typeDesc.Elements > 1)
		{
			return ShaderVariableClass::Array;
		}

		switch (typeDesc.Class)
		{
		case D3D_SVC_SCALAR:
			return ShaderVariableClass::Scalar;
		case D3D_SVC_VECTOR:
			return ShaderVariableClass::Vector;
		case D3D_SVC_MATRIX_ROWS:
		case D3D_SVC_MATRIX_COLUMNS:
			return ShaderVariableClass::Matrix;
		case D3D_SVC_STRUCT:
			return ShaderVariableClass::Struct;
		default:
			return ShaderVariableClass::Unknown;
		}
	}

	[[nodiscard]] std::uint32_t EstimateTypeSize(const D3D12_SHADER_TYPE_DESC& typeDesc)
	{
		std::uint32_t scalarSize = 4;
		switch (typeDesc.Type)
		{
		case D3D_SVT_DOUBLE:
			scalarSize = 8;
			break;
		case D3D_SVT_BOOL:
		case D3D_SVT_INT:
		case D3D_SVT_FLOAT:
		case D3D_SVT_UINT:
		case D3D_SVT_UINT8:
		case D3D_SVT_MIN16FLOAT:
			scalarSize = 4;
			break;
		default:
			break;
		}

		if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS)
		{
			return typeDesc.Rows * typeDesc.Columns * scalarSize;
		}

		if (typeDesc.Class == D3D_SVC_VECTOR)
		{
			return typeDesc.Columns * scalarSize;
		}

		if (typeDesc.Class == D3D_SVC_SCALAR)
		{
			return scalarSize;
		}

		return 0;
	}

	[[nodiscard]] ShaderVariableType ToShaderVariableType(const D3D12_SHADER_TYPE_DESC& typeDesc)
	{
		if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS)
		{
			if (typeDesc.Rows == 4 && typeDesc.Columns == 4)
			{
				return ShaderVariableType::Float4x4;
			}
			if (typeDesc.Rows == 3 && typeDesc.Columns == 3)
			{
				return ShaderVariableType::Float3x3;
			}
			if (typeDesc.Rows == 2 && typeDesc.Columns == 2)
			{
				return ShaderVariableType::Float2x2;
			}
		}

		const std::uint32_t componentCount = typeDesc.Columns;

		switch (typeDesc.Type)
		{
		case D3D_SVT_BOOL:
			return ShaderVariableType::Bool;

		case D3D_SVT_INT:
			switch (componentCount)
			{
			case 1:
				return ShaderVariableType::Int;
			case 2:
				return ShaderVariableType::Int2;
			case 3:
				return ShaderVariableType::Int3;
			case 4:
				return ShaderVariableType::Int4;
			default:
				break;
			}
			break;

		case D3D_SVT_UINT:
		case D3D_SVT_UINT8:
			switch (componentCount)
			{
			case 1:
				return ShaderVariableType::UInt;
			case 2:
				return ShaderVariableType::UInt2;
			case 3:
				return ShaderVariableType::UInt3;
			case 4:
				return ShaderVariableType::UInt4;
			default:
				break;
			}
			break;

		case D3D_SVT_FLOAT:
		case D3D_SVT_MIN16FLOAT:
			switch (componentCount)
			{
			case 1:
				return ShaderVariableType::Float;
			case 2:
				return ShaderVariableType::Float2;
			case 3:
				return ShaderVariableType::Float3;
			case 4:
				return ShaderVariableType::Float4;
			default:
				break;
			}
			break;

		default:
			break;
		}

		return ShaderVariableType::Unknown;
	}

	[[nodiscard]] std::uint32_t CountMaskComponents(std::uint8_t mask)
	{
		std::uint32_t count = 0;
		for (std::uint32_t bit = 0; bit < 4; ++bit)
		{
			if ((mask & (1u << bit)) != 0)
			{
				++count;
			}
		}
		return count;
	}

	[[nodiscard]] ShaderInputType ToShaderInputType(const D3D12_SIGNATURE_PARAMETER_DESC& parameterDesc)
	{
		const std::uint32_t componentCount = CountMaskComponents(static_cast<std::uint8_t>(parameterDesc.Mask));

		switch (parameterDesc.ComponentType)
		{
		case D3D_REGISTER_COMPONENT_UINT32:
			switch (componentCount)
			{
			case 1:
				return ShaderInputType::UInt;
			case 2:
				return ShaderInputType::UInt2;
			case 3:
				return ShaderInputType::UInt3;
			case 4:
				return ShaderInputType::UInt4;
			default:
				break;
			}
			break;

		case D3D_REGISTER_COMPONENT_SINT32:
			switch (componentCount)
			{
			case 1:
				return ShaderInputType::Int;
			case 2:
				return ShaderInputType::Int2;
			case 3:
				return ShaderInputType::Int3;
			case 4:
				return ShaderInputType::Int4;
			default:
				break;
			}
			break;

		case D3D_REGISTER_COMPONENT_FLOAT32:
			switch (componentCount)
			{
			case 1:
				return ShaderInputType::Float;
			case 2:
				return ShaderInputType::Float2;
			case 3:
				return ShaderInputType::Float3;
			case 4:
				return ShaderInputType::Float4;
			default:
				break;
			}
			break;

		default:
			break;
		}

		return ShaderInputType::Unknown;
	}

	[[nodiscard]] ShaderConstantVariable ConvertShaderType(
		ID3D12ShaderReflectionType* typeReflection,
		const char* name,
		std::uint32_t offset,
		std::uint32_t size)
	{
		ShaderConstantVariable variable{};
		if (name != nullptr)
		{
			variable.Name = name;
		}
		variable.Offset = offset;
		variable.Size = size;

		if (typeReflection == nullptr)
		{
			return variable;
		}

		D3D12_SHADER_TYPE_DESC typeDesc{};
		if (FAILED(typeReflection->GetDesc(&typeDesc)))
		{
			return variable;
		}

		variable.VariableClass = ToShaderVariableClass(typeDesc);
		variable.Type = ToShaderVariableType(typeDesc);
		variable.AlignedSize = size > 0 ? size : EstimateTypeSize(typeDesc);
		variable.ArraySize = typeDesc.Elements > 0 ? typeDesc.Elements : 1;

		if (typeDesc.Class == D3D_SVC_STRUCT)
		{
			variable.Members.reserve(typeDesc.Members);
			for (UINT memberIndex = 0; memberIndex < typeDesc.Members; ++memberIndex)
			{
				ID3D12ShaderReflectionType* memberTypeReflection = typeReflection->GetMemberTypeByIndex(memberIndex);
				if (memberTypeReflection == nullptr)
				{
					continue;
				}

				D3D12_SHADER_TYPE_DESC memberTypeDesc{};
				if (FAILED(memberTypeReflection->GetDesc(&memberTypeDesc)))
				{
					continue;
				}

				const char* memberName = typeReflection->GetMemberTypeName(memberIndex);
				const std::uint32_t memberSize = EstimateTypeSize(memberTypeDesc);
				variable.Members.push_back(ConvertShaderType(
					memberTypeReflection,
					memberName,
					memberTypeDesc.Offset,
					memberSize));
			}
		}

		return variable;
	}

	[[nodiscard]] ShaderConstantVariable ConvertShaderVariable(
		ID3D12ShaderReflectionVariable* variableReflection)
	{
		ShaderConstantVariable variable{};
		if (variableReflection == nullptr)
		{
			return variable;
		}

		D3D12_SHADER_VARIABLE_DESC variableDesc{};
		if (FAILED(variableReflection->GetDesc(&variableDesc)))
		{
			return variable;
		}

		return ConvertShaderType(
			variableReflection->GetType(),
			variableDesc.Name,
			variableDesc.StartOffset,
			variableDesc.Size);
	}
}

ShaderReflectionData ConvertD3D12ShaderReflection(
	ID3D12ShaderReflection* reflection,
	ShaderStage stage)
{
	ShaderReflectionData data{};
	data.Stage = stage;

	if (reflection == nullptr)
	{
		return data;
	}

	D3D12_SHADER_DESC shaderDesc{};
	if (FAILED(reflection->GetDesc(&shaderDesc)))
	{
		return data;
	}

	const ShaderStageFlags stageFlags = ToShaderStageFlags(stage);

	data.Bindings.reserve(shaderDesc.BoundResources);
	for (UINT resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
		if (FAILED(reflection->GetResourceBindingDesc(resourceIndex, &bindDesc)))
		{
			continue;
		}

		ShaderResourceBinding binding{};
		if (bindDesc.Name != nullptr)
		{
			binding.Name = bindDesc.Name;
		}
		binding.Type = ToShaderResourceType(bindDesc);
		binding.Access = ToShaderResourceAccess(bindDesc);
		binding.Visibility = stageFlags;
		binding.Register = bindDesc.BindPoint;
		binding.Space = bindDesc.Space;
		binding.BindCount = bindDesc.BindCount;
		binding.IsBindless = bindDesc.BindCount == UINT_MAX;
		data.Bindings.push_back(std::move(binding));
	}

	data.ConstantBuffers.reserve(shaderDesc.ConstantBuffers);
	for (UINT constantBufferIndex = 0; constantBufferIndex < shaderDesc.ConstantBuffers; ++constantBufferIndex)
	{
		ID3D12ShaderReflectionConstantBuffer* constantBufferReflection =
			reflection->GetConstantBufferByIndex(constantBufferIndex);
		if (constantBufferReflection == nullptr)
		{
			continue;
		}

		D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
		if (FAILED(constantBufferReflection->GetDesc(&constantBufferDesc)))
		{
			continue;
		}

		ShaderConstantBuffer constantBuffer{};
		if (constantBufferDesc.Name != nullptr)
		{
			constantBuffer.Name = constantBufferDesc.Name;
		}
		constantBuffer.Size = constantBufferDesc.Size;
		constantBuffer.AlignedSize = constantBufferDesc.Size;
		constantBuffer.Variables.reserve(constantBufferDesc.Variables);

		for (UINT variableIndex = 0; variableIndex < constantBufferDesc.Variables; ++variableIndex)
		{
			ID3D12ShaderReflectionVariable* variableReflection =
				constantBufferReflection->GetVariableByIndex(variableIndex);
			if (variableReflection == nullptr)
			{
				continue;
			}

			constantBuffer.Variables.push_back(ConvertShaderVariable(variableReflection));
		}

		for (const ShaderResourceBinding& binding : data.Bindings)
		{
			if (binding.Type == ShaderResourceType::ConstantBuffer && binding.Name == constantBuffer.Name)
			{
				constantBuffer.Register = binding.Register;
				constantBuffer.Space = binding.Space;
				break;
			}
		}

		data.ConstantBuffers.push_back(std::move(constantBuffer));
	}

	data.InputElements.reserve(shaderDesc.InputParameters);
	for (UINT parameterIndex = 0; parameterIndex < shaderDesc.InputParameters; ++parameterIndex)
	{
		D3D12_SIGNATURE_PARAMETER_DESC parameterDesc{};
		if (FAILED(reflection->GetInputParameterDesc(parameterIndex, &parameterDesc)))
		{
			continue;
		}

		ShaderInputElement inputElement{};
		if (parameterDesc.SemanticName != nullptr)
		{
			inputElement.SemanticName = parameterDesc.SemanticName;
		}
		inputElement.SemanticIndex = parameterDesc.SemanticIndex;
		inputElement.Type = ToShaderInputType(parameterDesc);
		inputElement.Register = parameterDesc.Register;
		inputElement.Stream = parameterDesc.Stream;
		inputElement.Classification = ShaderInputClassification::PerVertexData;
		inputElement.InstanceStepRate = 0;
		data.InputElements.push_back(std::move(inputElement));
	}

	if (stage == ShaderStage::Compute)
	{
		data.Compute.ThreadGroupX = 1;
		data.Compute.ThreadGroupY = 1;
		data.Compute.ThreadGroupZ = 1;
	}

	return data;
}

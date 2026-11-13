#pragma once


class RHIShader {
public:
    virtual ~RHIShader() = default;
};

class RHIVertexShader : public RHIShader {};
class RHIPixelShader : public RHIShader {};
#pragma once

enum class RaytracingApiType {
  Fallback,
  DXR
};


class RaytracingApi
{
public:
  void CheckDXRSupport(IDXGIAdapter1* adapter)
  {
    // Fallback Layer uses an experimental feature and needs to be enabled before creating a D3D12 device.
    is_fallback_supported = EnableComputeRaytracingFallback(adapter);

    if (!is_fallback_supported)
    {
        OutputDebugString(
            L"Warning: Could not enable Compute Raytracing Fallback (D3D12EnableExperimentalFeatures() failed).\n" \
            L"         Possible reasons: your OS is not in developer mode.\n\n");
    }

    is_dxr_supported = IsDirectXRaytracingSupported(adapter);

    if (!is_dxr_supported)
    {
        OutputDebugString(L"Warning: DirectX Raytracing is not supported by your GPU and driver.\n\n");

        ThrowIfFalse(is_fallback_supported, 
            L"Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures() failed).\n"\
            L"Possible reasons: your OS is not in developer mode.\n\n");
    }
    else
    {
      //select dxr
      is_fallback = false;
    }
  }

  void ForceSelectRaytracingAPI(RaytracingApiType type)
  {

    if (type == RaytracingApiType::Fallback)
    {
        is_fallback = true;
    }
    else // DirectX Raytracing
    {
        if (is_dxr_supported)
        {
            is_fallback = false;
        }
        else
        {
            OutputDebugString(L"Invalid selection - DXR is not available.\n");
        }
    }
  }

  friend bool operator==(const RaytracingApi& lhs, RaytracingApiType rhs)
  {
    if (rhs == RaytracingApiType::Fallback)
    {
      return lhs.is_fallback;
    }
    return !lhs.is_fallback;
  }

  RaytracingApiType GetRayTracingApiType() const
  {
    return is_fallback ? RaytracingApiType::Fallback : RaytracingApiType::DXR;
  }

private:
  bool is_fallback = true;

  bool is_fallback_supported = false;
  bool is_dxr_supported = false;
};

#include "RenderTexture.h"
#include "time/Time.h"

namespace Viry3D
{
	Map<long long, List<RenderTexture::Temporary>> RenderTexture::m_temporarys;

	void RenderTexture::Init()
	{
	}

	void RenderTexture::Deinit()
	{
		m_temporarys.Clear();
	}

	Ref<RenderTexture> RenderTexture::GetTemporary(int width,
		int height,
		RenderTextureFormat::Enum format,
		DepthBuffer::Enum depth,
		FilterMode::Enum filter_mode)
	{
		Ref<RenderTexture> texture;

		long long w = width;
		long long h = height;
		long long f = format;
		long long d = depth;
		long long key = (w << 0) | (h << 16) | (f << 32) | (d << 48);

		List<Temporary> *list;
		if(m_temporarys.TryGet(key, &list))
		{
			for(auto& i : *list)
			{
				if(!i.in_use)
				{
					texture = i.texture;
					i.in_use = true;

					if(texture->GetFilterMode() != filter_mode)
					{
						texture->SetFilterMode(filter_mode);
						texture->UpdateSampler();
					}
					break;
				}
			}
		}

		if(!texture)
		{
			texture = Create(width, height, format, depth, filter_mode);

			Temporary t;
			t.texture = texture;
			t.in_use = true;

			if(list != NULL)
			{
				list->AddLast(t);
			}
			else
			{
				List<Temporary> new_list;
				new_list.AddLast(t);
				m_temporarys.Add(key, new_list);
			}
		}

		return texture;
	}

	void RenderTexture::ReleaseTemporary(Ref<RenderTexture> texture)
	{
		long long w = texture->GetWidth();
		long long h = texture->GetHeight();
		long long f = texture->GetFormat();
		long long d = texture->GetDepth();
		long long key = (w << 0) | (h << 16) | (f << 32) | (d << 48);

		List<Temporary> *list;
		if(m_temporarys.TryGet(key, &list))
		{
			for(auto& i : *list)
			{
				if(i.texture == texture)
				{
					i.in_use = false;
					i.used_time = Time::GetRealTimeSinceStartup();
					break;
				}
			}
		}
	}

	Ref<RenderTexture> RenderTexture::Create(
		int width,
		int height,
		RenderTextureFormat::Enum format,
		DepthBuffer::Enum depth,
		FilterMode::Enum filter_mode)
	{
		Ref<RenderTexture> texture = Ref<RenderTexture>(new RenderTexture());
		texture->SetWidth(width);
		texture->SetHeight(height);
		texture->SetFormat(format);
		texture->SetDepth(depth);
		texture->SetWrapMode(TextureWrapMode::Clamp);
		texture->SetFilterMode(filter_mode);

		if(format == RenderTextureFormat::Depth)
		{
			texture->CreateDepthRenderTexture();
		}
		else
		{
			texture->CreateColorRenderTexture();
		}

		return texture;
	}

	RenderTexture::RenderTexture()
	{
		SetName("RenderTexture");
	}
}
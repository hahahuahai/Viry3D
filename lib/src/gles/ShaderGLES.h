#pragma once

#include "Object.h"
#include "gles_include.h"
#include "math/Matrix4x4.h"

namespace Viry3D
{
	class UniformBuffer;
	struct XMLUniformBuffer;
	struct XMLSampler;
	struct XMLVertexShader;

	struct GLRenderState
	{
		bool offset_enable;			//	GL_POLYGON_OFFSET_FILL
		GLfloat offset_factor;		//	glPolygonOffset
		GLfloat offset_units;
		bool cull_enable;			//	GL_CULL_FACE
		GLenum cull_face;			//	glCullFace
		GLboolean color_mask_r;		//	glColorMask
		GLboolean color_mask_g;
		GLboolean color_mask_b;
		GLboolean color_mask_a;
		bool blend_enable;			//	GL_BLEND
		GLenum blend_src_c;			//	glBlendFuncSeparate
		GLenum blend_dst_c;
		GLenum blend_src_a;
		GLenum blend_dst_a;
		GLboolean depth_mask;		//	glDepthMask
		GLenum depth_func;			//	glDepthFunc	
		bool stencil_enable;		//	GL_STENCIL_TEST
		GLenum stencil_func;		//	glStencilFunc
		GLint stencil_ref;
		GLuint stencil_read_mask;
		GLuint stencil_write_mask;	//	glStencilMask
		GLenum stencil_op_fail;		//	glStencilOp
		GLenum stencil_op_zfail;
		GLenum stencil_op_pass;
	};

	struct ShaderPass
	{
		String name;
		GLuint program;
		Ref<UniformBuffer> push_buffer;
		int push_buffer_size;
		Vector<XMLUniformBuffer*> uniform_buffer_infos;
		Vector<const XMLSampler*> sampler_infos;
		Vector<GLint> sampler_locations;
		const XMLVertexShader* vs;
		GLRenderState render_state;
	};

	class Material;

	class ShaderGLES : public Object
	{
	public:
		virtual ~ShaderGLES();

		int GetPassCount() const { return 1; }
		void ClearPipelines() { }
		void PreparePass(int index) { }
		void BeginPass(int index);
		void BindSharedMaterial(int index, const Ref<Material>& material);
		void BindMaterial(int index, const Ref<Material>& material, int lightmap_index) { }
		void BindLightmap(int index, const Ref<Material>& material, int lightmap_index);
		void PushConstant(int index, void* data, int size);
		void EndPass(int index) { }

		Ref<UniformBuffer> CreateUniformBuffer(int index);
		const Vector<const XMLSampler*>& GetSamplerInfos(int index) const { return m_passes[index].sampler_infos; }
		const Vector<GLint>& GetSamplerLocations(int index) const { return m_passes[index].sampler_locations; }
		const Vector<XMLUniformBuffer*>& GetUniformBufferInfos(int index) const { return m_passes[index].uniform_buffer_infos; }
		const XMLVertexShader* GetVertexShaderInfo(int index) const { return m_passes[index].vs; }

	protected:
		ShaderGLES();
		void Compile();

	private:
		void CreateShaders();
		void CreatePasses();

		Vector<ShaderPass> m_passes;
		Map<String, GLuint> m_vertex_shaders;
		Map<String, GLuint> m_pixel_shaders;
	};
}
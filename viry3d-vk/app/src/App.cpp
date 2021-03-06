/*
* Viry3D
* Copyright 2014-2018 by Stack - stackos@qq.com
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "App.h"
#include "Input.h"
#include "Debug.h"
#include "graphics/Display.h"
#include "graphics/Camera.h"
#include "graphics/Shader.h"
#include "graphics/Material.h"
#include "graphics/MeshRenderer.h"
#include "graphics/VertexAttribute.h"
#include "graphics/Mesh.h"
#include "graphics/Texture.h"
#include "memory/Memory.h"
#include "thread/ThreadPool.h"
#include "math/Quaternion.h"
#include "time/Time.h"
#include "ui/CanvasRenderer.h"
#include "ui/Sprite.h"
#include "ui/Label.h"
#include "ui/Font.h"
#include "ui/Button.h"

// TODO:
// - SwitchControl
// - SliderControl
// - android project
// - mac project
// - ios project
// - ScrollView TabView TreeView

#define RENDER_TO_TEXTURE 0
#define SHOW_DEPTH 1
#define BLUR_COLOR 1

namespace Viry3D
{
    class AppImplement
    {
    public:
        struct CameraParam
        {
            Vector3 pos;
            Quaternion rot;
            float fov;
            float near_clip;
            float far_clip;
        };
        CameraParam m_camera_param = {
            Vector3(0, 0, -5),
            Quaternion::Identity(),
            45,
            1,
            1000
        };

        Camera* m_camera;
        MeshRenderer* m_renderer_cube;
        MeshRenderer* m_renderer_sky;
        Label* m_label;
        float m_deg = 0;

        void InitPostEffectBlur(const Ref<Texture>& color_texture)
        {
            int downsample = 2;
            float texel_offset = 1.6f;
            int iter_count = 3;
            float iter_step = 0.0f;

            int width = color_texture->GetWidth();
            int height = color_texture->GetHeight();
            for (int i = 0; i < downsample; ++i)
            {
                width >>= 1;
                height >>= 1;
            }

            auto color_texture_2 = Texture::CreateRenderTexture(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FILTER_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            auto color_texture_3 = Texture::CreateRenderTexture(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FILTER_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

            String vs = R"(
Input(0) vec4 a_pos;
Input(2) vec2 a_uv;

Output(0) vec2 v_uv;

void main()
{
	gl_Position = a_pos;
	v_uv = a_uv;

	vulkan_convert();
}
)";
            String fs = R"(
precision highp float;
      
UniformTexture(0, 0) uniform sampler2D u_texture;

UniformBuffer(0, 1) uniform UniformBuffer01
{
	vec4 u_texel_size;
} buf_0_1;

Input(0) vec2 v_uv;

Output(0) vec4 o_frag;

const float kernel[7] = float[7]( 0.0205, 0.0855, 0.232, 0.324, 0.232, 0.0855, 0.0205 );

void main()
{
    vec4 c = vec4(0.0);
    for (int i = 0; i < 7; ++i)
    {
        c += texture(u_texture, v_uv + buf_0_1.u_texel_size.xy * float(i - 3)) * kernel[i];
    }
    o_frag = c;
}
)";
            RenderState render_state;
            render_state.cull = RenderState::Cull::Off;
            render_state.zTest = RenderState::ZTest::Off;
            render_state.zWrite = RenderState::ZWrite::Off;

            auto shader = RefMake<Shader>(
                vs,
                Vector<String>(),
                fs,
                Vector<String>(),
                render_state);

            int camera_depth = 2;

            // color -> color2, down sample
            auto blit_color_camera = Display::Instance()->CreateBlitCamera(camera_depth++, color_texture);
            blit_color_camera->SetRenderTarget(color_texture_2, Ref<Texture>());

            for (int i = 0; i < iter_count; ++i)
            {
                // color2 -> color, h blur
                auto material_h = RefMake<Material>(shader);
                material_h->SetVector("u_texel_size", Vector4(1.0f / width * texel_offset * (1.0f + i * iter_step), 0, 0, 0));

                blit_color_camera = Display::Instance()->CreateBlitCamera(camera_depth++, color_texture_2, material_h);
                blit_color_camera->SetRenderTarget(color_texture, Ref<Texture>());

                // color -> color2, v blur
                auto material_v = RefMake<Material>(shader);
                material_v->SetVector("u_texel_size", Vector4(0, 1.0f / height * texel_offset * (1.0f + i * iter_step), 0, 0));

                blit_color_camera = Display::Instance()->CreateBlitCamera(camera_depth++, color_texture, material_v);
                blit_color_camera->SetRenderTarget(color_texture_2, Ref<Texture>());
            }

            // color2 -> color, output
            blit_color_camera = Display::Instance()->CreateBlitCamera(camera_depth++, color_texture_2);
            blit_color_camera->SetRenderTarget(color_texture, Ref<Texture>());
        }

        AppImplement()
        {
        
        }

        void Init()
        {
            m_camera = Display::Instance()->CreateCamera();

#if RENDER_TO_TEXTURE
            m_camera->SetDepth(0);
            auto color_texture = Texture::CreateRenderTexture(
                1280,
                720,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FILTER_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            auto depth_texture = Texture::CreateRenderTexture(
                1280,
                720,
                VK_FORMAT_X8_D24_UNORM_PACK32,
                VK_FILTER_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            m_camera->SetRenderTarget(color_texture, depth_texture);

#if SHOW_DEPTH
            // depth -> color
            auto blit_depth_camera = Display::Instance()->CreateBlitCamera(1, depth_texture, Ref<Material>(), "", CameraClearFlags::Nothing, Rect(0.75f, 0, 0.25f, 0.25f));
            blit_depth_camera->SetRenderTarget(color_texture, Ref<Texture>());
#endif

#if BLUR_COLOR
            this->InitPostEffectBlur(color_texture);
#endif

            // color -> window
            Display::Instance()->CreateBlitCamera(0x7fffffff, color_texture);
#endif

            String vs = R"(
UniformBuffer(0, 0) uniform UniformBuffer00
{
	mat4 u_view_matrix;
	mat4 u_projection_matrix;
} buf_0_0;

UniformBuffer(1, 0) uniform UniformBuffer10
{
	mat4 u_model_matrix;
} buf_1_0;

Input(0) vec4 a_pos;
Input(2) vec2 a_uv;

Output(0) vec2 v_uv;

void main()
{
	gl_Position = a_pos * buf_1_0.u_model_matrix * buf_0_0.u_view_matrix * buf_0_0.u_projection_matrix;
	v_uv = a_uv;

	vulkan_convert();
}
)";
            String fs = R"(
precision highp float;
      
UniformTexture(0, 1) uniform sampler2D u_texture;

Input(0) vec2 v_uv;

Output(0) vec4 o_frag;

void main()
{
    o_frag = texture(u_texture, v_uv);
}
)";
            RenderState render_state;

            auto shader = RefMake<Shader>(
                vs,
                Vector<String>(),
                fs,
                Vector<String>(),
                render_state);
            auto material = RefMake<Material>(shader);

            Vector<Vertex> vertices(8);
            Memory::Zero(&vertices[0], vertices.SizeInBytes());
            vertices[0].vertex = Vector3(-0.5f, 0.5f, -0.5f);
            vertices[1].vertex = Vector3(-0.5f, -0.5f, -0.5f);
            vertices[2].vertex = Vector3(0.5f, -0.5f, -0.5f);
            vertices[3].vertex = Vector3(0.5f, 0.5f, -0.5f);
            vertices[4].vertex = Vector3(-0.5f, 0.5f, 0.5f);
            vertices[5].vertex = Vector3(-0.5f, -0.5f, 0.5f);
            vertices[6].vertex = Vector3(0.5f, -0.5f, 0.5f);
            vertices[7].vertex = Vector3(0.5f, 0.5f, 0.5f);
            vertices[0].uv = Vector2(0, 0);
            vertices[1].uv = Vector2(0, 1);
            vertices[2].uv = Vector2(1, 1);
            vertices[3].uv = Vector2(1, 0);
            vertices[4].uv = Vector2(1, 0);
            vertices[5].uv = Vector2(1, 1);
            vertices[6].uv = Vector2(0, 1);
            vertices[7].uv = Vector2(0, 0);

            Vector<unsigned short> indices({
                0, 1, 2, 0, 2, 3,
                3, 2, 6, 3, 6, 7,
                7, 6, 5, 7, 5, 4,
                4, 5, 1, 4, 1, 0,
                4, 0, 3, 4, 3, 7,
                1, 5, 6, 1, 6, 2
                });
            auto mesh = RefMake<Mesh>(vertices, indices);

            auto renderer = RefMake<MeshRenderer>();
            renderer->SetMaterial(material);
            renderer->SetMesh(mesh);
            m_renderer_cube = renderer.get();

            m_camera->AddRenderer(renderer);

            auto texture = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/logo.jpg", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true);
            material->SetTexture("u_texture", texture);

            Vector3 camera_forward = m_camera_param.rot * Vector3(0, 0, 1);
            Vector3 camera_up = m_camera_param.rot * Vector3(0, 1, 0);
            Matrix4x4 view = Matrix4x4::LookTo(m_camera_param.pos, camera_forward, camera_up);
            Matrix4x4 projection = Matrix4x4::Perspective(m_camera_param.fov, m_camera->GetTargetWidth() / (float) m_camera->GetTargetHeight(), m_camera_param.near_clip, m_camera_param.far_clip);
            material->SetMatrix("u_view_matrix", view);
            material->SetMatrix("u_projection_matrix", projection);

            this->InitSkybox(mesh, view, projection);
            this->InitUI();
        }

        void InitSkybox(const Ref<Mesh>& mesh, const Matrix4x4& view, const Matrix4x4& projection)
        {
            String vs = R"(
UniformBuffer(0, 0) uniform UniformBuffer00
{
	mat4 u_view_matrix;
	mat4 u_projection_matrix;
} buf_0_0;

UniformBuffer(1, 0) uniform UniformBuffer10
{
	mat4 u_model_matrix;
} buf_1_0;

Input(0) vec4 a_pos;

Output(0) vec3 v_uv;

void main()
{
	gl_Position = (a_pos * buf_1_0.u_model_matrix * buf_0_0.u_view_matrix * buf_0_0.u_projection_matrix).xyww;
	v_uv = a_pos.xyz;

	vulkan_convert();
}
)";
            String fs = R"(
precision highp float;
      
UniformTexture(0, 1) uniform samplerCube u_texture;

Input(0) vec3 v_uv;

Output(0) vec4 o_frag;

void main()
{
    vec4 c = textureLod(u_texture, v_uv, 0.0);
    o_frag = pow(c, vec4(1.0 / 2.2));
}
)";
            RenderState render_state;
            render_state.cull = RenderState::Cull::Front;
            render_state.zWrite = RenderState::ZWrite::Off;
            render_state.queue = (int) RenderState::Queue::Background;
            auto shader = RefMake<Shader>(
                vs,
                Vector<String>(),
                fs,
                Vector<String>(),
                render_state);
            auto material = RefMake<Material>(shader);

            auto renderer = RefMake<MeshRenderer>();
            renderer->SetMaterial(material);
            renderer->SetMesh(mesh);
            m_renderer_sky = renderer.get();

            material->SetMatrix("u_view_matrix", view);
            material->SetMatrix("u_projection_matrix", projection);

            Matrix4x4 model = Matrix4x4::Translation(m_camera_param.pos);
            renderer->SetInstanceMatrix("u_model_matrix", model);

            Thread::Task task;
            task.job = []() {
                auto cubemap = Texture::CreateCubemap(1024, VK_FORMAT_R8G8B8A8_UNORM, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true);
                for (int i = 0; i < cubemap->GetMipmapLevelCount(); ++i)
                {
                    for (int j = 0; j < 6; ++j)
                    {
                        int width;
                        int height;
                        int bpp;
                        ByteBuffer pixels = Texture::LoadImageFromFile(String::Format((Application::Instance()->GetDataPath() + "/texture/prefilter/%d_%d.png").CString(), i, j), width, height, bpp);
                        cubemap->UpdateCubemap(pixels, (CubemapFace) j, i);
                    }
                }
                return cubemap;
            };
            task.complete = [=](const Ref<Thread::Res>& res) {
                material->SetTexture("u_texture", RefCast<Texture>(res));
                m_camera->AddRenderer(renderer);
            };
            Application::Instance()->GetThreadPool()->AddTask(task);
        }

        void InitUI()
        {
            auto canvas = RefMake<CanvasRenderer>();
            m_camera->AddRenderer(canvas);

            auto texture0 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/0.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
            auto texture1 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/1.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
            auto texture2 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/2.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
            auto texture3 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/3.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
            auto texture4 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/4.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
            auto texture5 = Texture::LoadTexture2DFromFile(Application::Instance()->GetDataPath() + "/texture/ui/5.png", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);

            auto sprite = RefMake<Sprite>();
            sprite->SetSize(Vector2i(100, 100));
            sprite->SetTexture(texture3);

            canvas->AddView(sprite);

            sprite = RefMake<Sprite>();
            sprite->SetSize(Vector2i(100, 100));
            sprite->SetOffset(Vector2i(400, 100));
            sprite->SetTexture(texture2);

            canvas->AddView(sprite);

            sprite = RefMake<Sprite>();
            sprite->SetSize(Vector2i(100, 100));
            sprite->SetOffset(Vector2i(-400, -100));
            sprite->SetTexture(texture4);

            canvas->AddView(sprite);

            sprite = RefMake<Sprite>();
            sprite->SetSize(Vector2i(100, 100));
            sprite->SetOffset(Vector2i(400, -100));
            sprite->SetTexture(texture1);
            sprite->SetLocalRotation(Quaternion::Euler(Vector3(0, 0, 45)));

            canvas->AddView(sprite);

            auto label = RefMake<Label>();
            label->SetAlignment(ViewAlignment::Left | ViewAlignment::Top);
            label->SetPivot(Vector2(0, 0));
            label->SetSize(Vector2i(100, 30));
            label->SetOffset(Vector2i(40, 40));
            label->SetFont(Font::GetFont(FontType::PingFangSC));
            label->SetFontSize(28);
            label->SetTextAlignment(ViewAlignment::Left | ViewAlignment::Top);

            m_label = label.get();

            canvas->AddView(label);

            auto button = RefMake<Button>();
            button->SetSize(Vector2i(100, 100));
            button->SetOffset(Vector2i(-400, 100));
            button->GetLabel()->SetText("button");
            button->SetOnClick([]() {
                Log("click button");
            });

            canvas->AddView(button);
        }

        ~AppImplement()
        {
            Display::Instance()->DestroyCamera(m_camera);
            m_camera = nullptr;
        }

        void Update()
        {
            m_deg += 0.1f;

            Matrix4x4 model = Matrix4x4::Rotation(Quaternion::Euler(Vector3(0, m_deg, 0)));
            m_renderer_cube->SetInstanceMatrix("u_model_matrix", model);

            m_label->SetText(String::Format("FPS:%d", Time::GetFPS()));
        }

        void OnResize(int width, int height)
        {
            Matrix4x4 projection = Matrix4x4::Perspective(m_camera_param.fov, m_camera->GetTargetWidth() / (float) m_camera->GetTargetHeight(), m_camera_param.near_clip, m_camera_param.far_clip);
            m_renderer_cube->GetMaterial()->SetMatrix("u_projection_matrix", projection);
            m_renderer_sky->GetMaterial()->SetMatrix("u_projection_matrix", projection);
        }
    };

    App::App()
    {
        m_app = new AppImplement();
    }

    App::~App()
    {
        delete m_app;
    }

    void App::Init()
    {
        m_app->Init();
    }

    void App::Update()
    {
        m_app->Update();
    }

    void App::OnResize(int width, int height)
    {
        m_app->OnResize(width, height);
    }
}

#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/Font.h"
#include "../../IgnisLib/Source/IgnisLib.h"

using namespace Ignis;

// Resize callback
void CursorMoved(Window window) {
    Vec2<double> curPos = Input::CursorPosition(window);
    std::cout << "Curent cursor position: " << curPos.x << "; " << curPos.y << std::endl;
}

struct UniformData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct TextUniformData {
    glm::vec3 pos;
    Vec2f size;
    Vec2f uvRect;
};

int main() {
    std::vector<int> surfaces;

    // need to initialize the input before using/
    Input::Init();
    Render::Init(true);

    // DSid
    std::vector<Render::DescriptorSetInfo> descSetInfos{
        {
            {
                Render::CreateImageDescriptor(0, 1028, Render::CreateSampler(), Render::ShaderStage::FRAGMENT),
            },
            0x0,
        },
        {
            {
                Render::CreateUniformDescriptor(0, sizeof(UniformData), Render::ShaderStage::VERTEX),
            },
            0x1,
        },
        {
            {
                Render::CreateUniformDescriptor(0, sizeof(UniformData), Render::ShaderStage::VERTEX),
            },
            0x1,
        }
    };

    Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");

    // need to initialize the window & hook a callback witch has a window as an input
    Input::InitWindow(window1);
    // Input::HookInputCursorPositionCallback(window1, CursorMoved);
    int surface = Render::CreateSurface(window1);
    std::vector<Render::DescriptorSetId> descIds;
    Render::CreateDescriptorSets(descIds, descSetInfos);

    CreateGraphicPipeLineInfo gpInfo{};
    gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
    gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";
    gpInfo.blendEnable = true;
    gpInfo.descriptorSetIds = {
        descIds[0], descIds[1]
    };
    gpInfo.constantsSize = sizeof(float);
    gpInfo.vertexDataLayout = Render::CreateVertexData(Render::VEC3, Render::VEC3, Render::VEC2, Render::UINT);
    CreateGraphicPipeLineInfo fontpInfo{};
    fontpInfo.vertexShader = "../Resources/Shaders/textShader.vert";
    fontpInfo.fragmentShader = "../Resources/Shaders/textShader.frag";
    fontpInfo.blendEnable = true;
    fontpInfo.descriptorSetIds = {
        descIds[0], descIds[2]
    };
    fontpInfo.constantsSize = sizeof(float);
    fontpInfo.vertexDataLayout = Render::CreateVertexData(Render::VEC3, Render::VEC3, Render::VEC2, Render::UINT);

    Render::CreatePipeline(surface, gpInfo);
    Render::CreatePipeline(surface, fontpInfo);
    surfaces.push_back(surface);

    int monika = Render::CreateTexture("../Resources/Textures/monika2.png");
    int sus = Render::CreateTexture("../Resources/Textures/goated0.bmp");

    int fontId = UI::LoadFont("../DejaVuSans.ttf");

    UI::SetMainSurface(surface);

    Color tip(0, 255, 0);
    Color base = tip.Inverted();

    // Text text = Text(Vec2f(40, 10), Vec2f(100,100), "heooo fak yeah", fontId);

    View view = View(Vec2f(40, 10), Vec2f(20, 20), base);
    Image image2 = Image(Vec2f(25, 40), Vec2f(50, 20), tip);
    Image image3 = Image(Vec2f(0, 0), Vec2f(100, 100), monika);
    view.Add(image2);
    UI::AddToSurface(surface, image3, view);
    UI::SubmitSurface();

    float color = 0.0;

    while (surfaces.size() != 0) {
        Input::Event();
        Render::Update();

        if (color > 1.0f) color = 0.0f;

        color += 0.004f;

        for (auto it = surfaces.begin(); it != surfaces.end();) {
            if (Render::IsValidSurface(*it)) {
                Render::PushConstants(*it, &color, sizeof(color));
                Render::Draw(*it);
                it++;
            } else
                it = surfaces.erase(it);
        }
    }

    UI::Clean();
}

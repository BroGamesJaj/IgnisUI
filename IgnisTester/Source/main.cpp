#include <cstdint>
#include "glm/ext/vector_float3.hpp"
#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"

using namespace Ignis;

// need to initialize the input before using
// => Input::Init();
// => Input::InitWindow(window1); maytodo: this init should be enough, we doesnt need the basic one, like with the ui
// Resize callback
// => Input::HookInputCursorPositionCallback(window1, CursorMoved);
// called in a for loop to update inputs
// => Input::Event();
void CursorMoved(Window window) {
    Vec2<double> curPos = Input::CursorPosition(window);
    std::cout << "Curent cursor position: " << curPos.x << "; " << curPos.y << std::endl;
}

struct Test {
  int a;
};

int main() {
    Input::Init();
    Render::Init(true);

    Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");

    //int fontId = UI::LoadFont("../DejaVuSans.ttf");
    //UI::Init(window1);
    //UI::SetMainWindow(window1);

    //Texture monika = UI::CreateTexture("../Resources/Textures/monika2.png");
    //Texture sus = UI::CreateTexture("../Resources/Textures/goated0.bmp");

      Render::DescriptorSetInfo descriptorSet = {
          {
                  Render::CreateImageDescriptor(0, 1028, Render::CreateSampler(), Render::ShaderStage::FRAGMENT),
          },
          0x0,
      };

      int mainDescriptor = Render::CreateDescriptorSet(descriptorSet);


      CreateGraphicPipeLineInfo gpInfo{};
      gpInfo.vertexShader = "../Resources/Shaders/tester.vert";
      gpInfo.fragmentShader = "../Resources/Shaders/tester.frag";
      gpInfo.blendEnable = true;
      gpInfo.descriptorSetIds = {
          mainDescriptor
      };
      gpInfo.vertexDataLayout = Render::CreateVertexData(Render::VEC3, Render::VEC3, Render::VEC2, Render::UINT);
      int pipeline = Render::CreatePipeline(gpInfo);

      //Render::Surface surface = Render::CreateSurface(window1, {pipeline});

      //std::cout << Render::IsValidSurface(surface) << std::endl;

      std::vector<Vertex> vertecies = {
        {.pos = glm::vec3(0.5,0.5,0)},
        {.pos = glm::vec3(0.5,-0.5,0)},
        {.pos = glm::vec3(-0.5,0.5,0)},
        {.pos = glm::vec3(-0.5,-0.5,0)},
      };

      std::vector<uint32_t> indicies = {
        2,1,0,2,1,4
      };

      VertexData data{
        .vertecies = vertecies,
        .indicies = indicies
      };

      //Render::PushOn(data, surface);
      //Render::Submit(surface);

    //Image image = Image(Vec2f(0,0), Vec2f(100,100), Color(1.0f,1.0f,1.0f));
    //Image image3 = Image(Vec2f(0, 0), Vec2f(100, 100), monika);
    //UI::PushOn(window1, image);
    //UI::Submit();

    
    while (true) {
      Input::Event();
    }


    float time = 0.0f;
    //while (UI::CanDraw()) {
        Input::Event();
        Render::Update();
//        Render::Draw(surface);
        //UI::Draw();
    //}
    
    //Render::Draw(surface);
    //UI::Clean();
    //Render::Clean();
}

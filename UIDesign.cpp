#include <vector>

enum ViewMode {
    IGNIS_VIEW_DISCRATE,
    IGNIS_VIEW_CONTINOUS,
};

enum Relatives {
    IGNIS_RELATIVE_NONE,
    IGNIS_RELATIVE_SCREEN,
    IGNIS_RELATIVE_WINDOW,
    IGNIS_RELATIVE_VIEW
};

typedef struct IgVec2{
    int x, y;
    IgVec2() = default;
    IgVec2(int xIn, int yIn) : x(xIn), y(yIn) {}
};

class UIElement{
private:
public:
    size_t id = -1;
    char* uid; 
    IgVec2 position;
    IgVec2 size;
    UIElement* father = NULL;

    virtual ~UIElement() = default;
};

class Button : public UIElement {
private:
public:
    void* callback;
    Button(IgVec2 positionIn, IgVec2 sizeIn, void(*func)) : callback(func) {
        this->position = positionIn;
        this->size = sizeIn;
    };

    Button(const char* uidIn, IgVec2 positionIn, IgVec2 sizeIn, void(*func)) : callback(func) {
        size_t len = strlen(uidIn) + 1;
        this->uid = new char[len];
        memcpy(this->uid, uidIn, len);
        this->position = positionIn;
        this->size = sizeIn;
    }

    ~Button() {
        if (uid){
            delete[] uid;
        }
    }
};

struct InsertionProxy {
    View& view;
    size_t index;

    InsertionProxy(View& v, size_t i) : view(v), index(i) {}

    void operator--(int)
    {
        view.elements.erase(view.elements.begin()+index);
    }

    InsertionProxy& operator<<(const UIElement el){
        if (index >= view.elements.size()) {
            view.elements.resize(index + 1);
        }
        view.elements.insert(view.elements.begin() + index + 1, el);
        index++;
        return *this;
    }
};

class View : public UIElement {
protected:
    ViewMode viewMode = IGNIS_VIEW_CONTINOUS;
    Relatives relative = IGNIS_RELATIVE_VIEW;
public:
    std::vector<UIElement> elements;

    View() = default;

    View(IgVec2 pos, IgVec2 size, ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) {
        this->position = pos;
        this->size = size;
        this->viewMode = viewMode;
        this->relative = relative;
    }

    InsertionProxy operator[](size_t index) {
        return InsertionProxy(*this, index);
    }

    View& operator<<(const UIElement el) {
        elements.push_back(el);
        return *this;
    }
};

class MainView : public View {
public:
    MainView(ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) {
        this->position = IgVec2(0,0);
        this->size = IgVec2(1920,1080); //windowsize
        this->viewMode = viewMode;
        this->relative = relative;
    }
};

class Ignis{
public:
    static void SetMainView(MainView mainView){

    }
    static void LoadView();
};


void ButtonOnclick(){
    printf("button pressed\n");
}

int main(){
    MainView indexView(IGNIS_VIEW_CONTINOUS, IGNIS_RELATIVE_VIEW);
    Ignis::SetMainView(indexView);
    
    View view(IgVec2(25,25), IgVec2(50,50), IGNIS_VIEW_CONTINOUS, IGNIS_RELATIVE_WINDOW);
    Button button = Button("My button", IgVec2(5,3), IgVec2(10,10), ButtonOnclick);

    indexView << view << button;

    indexView[3] << button;
    indexView[4]--;



    Ignis::LoadView();
}
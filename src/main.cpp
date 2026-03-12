#include "app/Application.h"

int main(int argc, char** argv)
{
    Application app;
    if (!app.initialize(argc, argv))
    {
        return 1;
    }
    return app.run();
}

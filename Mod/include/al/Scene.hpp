#pragma once

namespace al
{
    class Scene
    {
        Scene(const char* name);

        ~Scene() override;

        virtual void init(unsigned long* info) {}

        virtual void appear();
        virtual void kill();
    };
}

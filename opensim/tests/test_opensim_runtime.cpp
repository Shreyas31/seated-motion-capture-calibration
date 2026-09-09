#include <OpenSim/OpenSim.h>
#include <exception>
#include <iostream>

int main() {
    try {
        OpenSim::Model model;
        model.setName("SeatedMocap_OpenSim_Test");
        model.setUseVisualizer(false);

        auto& state = model.initSystem();

        std::cout << "OpenSim loaded successfully.\n"
                  << "Model: " << model.getName() << '\n'
                  << "Bodies: " << model.getBodySet().getSize() << '\n'
                  << "Initial time: " << state.getTime() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "OpenSim test failed: " << error.what() << '\n';

        return 1;
    }
}

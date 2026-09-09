#include "realtime_viewer_configuration.h"
#include "realtime_viewer_runner.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        if (!SeatedMoCap::isRealtimeViewerArgumentCountValid(argc)) {
            std::cerr << SeatedMoCap::realtimeViewerUsage();
            return 2;
        }

        const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(argc, argv);
        SeatedMoCap::validateRealtimeViewerInputPaths(configuration);

        SeatedMoCap::RealtimeViewerRunner runner{configuration, std::cout, std::cerr};
        const auto statistics = runner.run();
        (void)statistics;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Real-time viewer test failed: " << error.what() << '\n';
        return 1;
    }
}

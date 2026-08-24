#include "previewer.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: gmplugin-preview-cli plugin.gmp [--frames N] [--pgm output.pgm]\n"
                     "       [--button ACTION] [--gesture ID] [--direction ID]\n"
                     "       [--bt CHANNEL TEXT]\n";
        return 2;
    }
    int frames = 3;
    std::string pgm;
    int button = 0, gesture = 0, direction = 0, bt_channel = -1;
    std::string bt_text;
    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (option == "--pgm" && i + 1 < argc) pgm = argv[++i];
        else if (option == "--button" && i + 1 < argc) button = std::atoi(argv[++i]);
        else if (option == "--gesture" && i + 1 < argc) gesture = std::atoi(argv[++i]);
        else if (option == "--direction" && i + 1 < argc) direction = std::atoi(argv[++i]);
        else if (option == "--bt" && i + 2 < argc) {
            bt_channel = std::atoi(argv[++i]);
            bt_text = argv[++i];
        }
        else {
            std::cerr << "Unknown option: " << option << "\n";
            return 2;
        }
    }
    try {
        gmpreview::Previewer previewer;
        previewer.loadFile(argv[1]);
        previewer.start();
        if (button) previewer.sendButton(static_cast<uint16_t>(button));
        if (gesture) previewer.sendGesture(static_cast<uint16_t>(gesture));
        if (direction) previewer.simulateDirectionGesture(static_cast<uint16_t>(direction));
        if (bt_channel >= 0) previewer.sendBluetooth(static_cast<uint16_t>(bt_channel),
            std::vector<uint8_t>(bt_text.begin(), bt_text.end()));
        for (int i = 0; i < frames; ++i) previewer.tick(33);
        previewer.renderFrame();
        if (!pgm.empty()) previewer.savePgm(pgm);
        for (const std::string &line : previewer.logs()) std::cout << line << "\n";
        const auto &info = previewer.packageInfo();
        std::cout << "[Previewer] OK objects=" << previewer.objectCount()
                  << " overlays=" << previewer.textOverlays().size()
                  << " image=" << info.image_size
                  << " memory=" << info.memory_size
                  << " instructions=" << previewer.instructionCount()
                  << "\n";
        previewer.stop();
        previewer.unload();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "GM Plugin Previewer error: " << error.what() << "\n";
        return 1;
    }
}

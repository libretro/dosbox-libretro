#include "Aulib/DecoderBassmidi.h"
#include "Aulib/DecoderFluidsynth.h"
#include "Aulib/DecoderSndfile.h"
#include "Aulib/DecoderWildmidi.h"
#include "Aulib/ResamplerSpeex.h"
#include "Aulib/Stream.h"
#include <SDL.h>
#include <iostream>
#include <memory>
//#include "filedetector.h"

using namespace Aulib;

auto main(int /*argc*/, char* argv[]) -> int
{
    init(44100, AUDIO_S16SYS, 2, 4096);

    DecoderBassmidi::setDefaultSoundfont("/usr/local/share/soundfonts/gs.sf2");
    DecoderWildmidi::init("/usr/share/timidity/current/timidity.cfg", 44100, true, true);

    auto decoder = Decoder::decoderFor(argv[1]);
    if (decoder == nullptr) {
        std::cerr << "No decoder found.\n";
        return 1;
    }

    auto fsynth = dynamic_cast<DecoderFluidsynth*>(decoder.get());
    // auto bassmidi = dynamic_cast<DecoderBassmidi*>(decoder.get());
    if (fsynth != nullptr) {
        fsynth->loadSoundfont("/usr/local/share/soundfonts/gs.sf2");
    }

    Stream stream(argv[1], std::move(decoder), std::make_unique<ResamplerSpeex>());
    stream.play();
    while (stream.isPlaying()) {
        SDL_Delay(200);
    }
    Aulib::quit();
}

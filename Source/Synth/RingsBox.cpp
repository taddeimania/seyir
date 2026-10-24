#include "RingsBox.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "rings/dsp/fx/reverb.h"
#include "rings/dsp/part.h"

namespace seyir
{
struct RingsBox::Impl
{
    rings::Part part;
    rings::Reverb verb;
    uint16_t partRam[32768];
    uint16_t verbRam[32768];
    int model = -1;
};

RingsBox::RingsBox() : impl (std::make_unique<Impl>())
{
    std::memset (impl->partRam, 0, sizeof (impl->partRam));
    std::memset (impl->verbRam, 0, sizeof (impl->verbRam));
    impl->part.Init (impl->partRam);
    impl->part.set_polyphony (2);
    impl->verb.Init (impl->verbRam);
    setModel (Modal);
}

RingsBox::~RingsBox() = default;

void RingsBox::setModel (int model)
{
    static const rings::ResonatorModel models[] {
        rings::RESONATOR_MODEL_MODAL,
        rings::RESONATOR_MODEL_SYMPATHETIC_STRING,
        rings::RESONATOR_MODEL_STRING,
        rings::RESONATOR_MODEL_FM_VOICE,
    };
    model = std::clamp (model, 0, 3);
    if (model != impl->model)
    {
        impl->model = model;
        impl->part.set_model (models[model]);
    }
}

void RingsBox::resonate (bool strum, float note, float structure, float brightness, float damping,
                         float position, float* left, float* right, int size)
{
    rings::Patch patch {};
    patch.structure = structure;
    patch.brightness = brightness;
    patch.damping = damping;
    patch.position = position;

    rings::PerformanceState ps {};
    ps.strum = strum;
    ps.internal_exciter = true;
    ps.internal_strum = false;
    ps.internal_note = false;
    ps.tonic = 0.0f;
    ps.note = note;
    ps.fm = 0.0f;
    ps.chord = std::clamp ((int) std::lround (structure * (rings::kNumChords - 1)), 0, rings::kNumChords - 1);

    float in[rings::kMaxBlockSize] {};
    impl->part.Process (ps, patch, in, left, right, (size_t) std::min (size, (int) rings::kMaxBlockSize));
}

void RingsBox::reverb (float* left, float* right, int size, float amount, float time, float lp)
{
    auto& v = impl->verb;
    v.set_amount (amount);
    v.set_time (time);
    v.set_diffusion (0.625f);
    v.set_input_gain (0.2f);
    v.set_lp (lp);
    v.Process (left, right, (size_t) size);
}
} // namespace seyir

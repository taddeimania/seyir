// RingsBox.h — Rings' resonator and reverb behind a small interface.
// Plaits and Rings define lookup tables under the same macro names with
// different sizes, so each lives in its own translation unit.
#pragma once

#include <memory>

namespace seyir
{
class RingsBox
{
public:
    RingsBox();
    ~RingsBox();

    // Resonator models, in Rings' order.
    enum Model { Modal, SympatheticString, String, FmVoice };

    void setModel (int model);

    // One block of the resonator, struck by its internal exciter when `strum`.
    void resonate (bool strum, float note, float structure, float brightness, float damping,
                   float position, float* left, float* right, int size);

    // Rings' reverb, in place.
    void reverb (float* left, float* right, int size, float amount, float time, float lp);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace seyir

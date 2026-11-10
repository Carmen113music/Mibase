#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include "EstatEstudi.h"
#include <mibase/MotorEstudi.h>

/** 0.28.1: llegir les preses d'un projecte (l'àudio sencer per sonar i l'ona per dibuixar). Es pot fer des de
    qualsevol fil (amb el seu AudioFormatManager): en obrir un projecte es fa en segon pla perquè la finestra de càrrega
    es vegi i Windows no digui que el programa no respon. */
struct PresaCarregada
{
    std::shared_ptr<OnaPresa> ona;
    std::shared_ptr<mibase::AudioPresa> audio;
};
using Precarrega = std::map<std::string, PresaCarregada>;

inline PresaCarregada llegeixPresa (juce::AudioFormatManager& fm, const juce::File& fitxer,
                                    const std::function<void (double)>& progres, const std::atomic<bool>* atura)
{
    PresaCarregada r;
    auto ona = std::make_shared<OnaPresa>();
    ona->lector.reset (fm.createReaderFor (fitxer));
    if (ona->lector == nullptr || ona->lector->sampleRate <= 0.0) return r;

    auto& lector = *ona->lector;
    ona->sr = lector.sampleRate;
    ona->longitud = lector.lengthInSamples;
    ona->canals = (int) lector.numChannels;
    const auto blocs = (size_t) ((ona->longitud + OnaPresa::bloc - 1) / OnaPresa::bloc);
    ona->mins.assign (blocs, 0.0f);
    ona->maxs.assign (blocs, 0.0f);

    // L'àudio sencer a la memòria (per sonar) i, alhora, l'ona per dibuixar.
    auto audio = std::make_shared<mibase::AudioPresa>();
    audio->fs = lector.sampleRate;
    audio->canals = lector.numChannels > 1 ? 2 : 1;
    audio->longitud = ona->longitud;
    for (int c = 0; c < audio->canals; ++c)
        audio->dades[c].assign ((size_t) audio->longitud, 0.0f);

    const int tros = OnaPresa::bloc * 512;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, lector.numChannels), tros);
    const float factor = 1.0f / (float) buffer.getNumChannels();
    for (juce::int64 pos = 0; pos < ona->longitud; pos += tros)
    {
        if (atura != nullptr && atura->load()) return {};
        const int n = (int) juce::jmin ((juce::int64) tros, ona->longitud - pos);
        lector.read (&buffer, 0, n, pos, true, true);
        if (progres) progres ((double) (pos + n) / (double) juce::jmax ((juce::int64) 1, ona->longitud));
        for (int c = 0; c < audio->canals; ++c)
            std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + n, audio->dades[c].data() + pos);
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) v += buffer.getReadPointer (ch)[i];
            v *= factor;
            const auto b = (size_t) ((pos + i) / OnaPresa::bloc);
            ona->mins[b] = juce::jmin (ona->mins[b], v);
            ona->maxs[b] = juce::jmax (ona->maxs[b], v);
        }
    }
    r.ona = std::move (ona);
    r.audio = std::move (audio);
    return r;
}

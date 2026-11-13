#include "mibase/GeneralMidi.h"

#include <algorithm>
#include <cstring>

#if defined(_MSC_VER)
 #pragma warning(push, 0)
#elif defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wsign-compare"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define TSF_IMPLEMENTATION
#define TSF_STATIC
#include "tsf.h"
#if defined(_MSC_VER)
 #pragma warning(pop)
#elif defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif

namespace mibase::gm
{
    class Banc
    {
    public:
        explicit Banc (tsf* t) : f (t) {}
        ~Banc() { if (f != nullptr) tsf_close (f); }
        tsf* f = nullptr;
    };

    std::shared_ptr<Banc> llegeix (const std::string& cami)
    {
        tsf* f = tsf_load_filename (cami.c_str());
        return f != nullptr ? std::make_shared<Banc> (f) : nullptr;
    }

    std::shared_ptr<Banc> llegeixMemoria (const void* dades, int mida)
    {
        tsf* f = tsf_load_memory (dades, mida);
        return f != nullptr ? std::make_shared<Banc> (f) : nullptr;
    }

    int numPresets (const Banc& b) { return b.f != nullptr ? tsf_get_presetcount (b.f) : 0; }

    int programa (const std::string& instrument)
    {
        if (instrument.rfind ("gm:", 0) != 0 || instrument.size() < 4) return -1;
        int v = 0;
        for (size_t i = 3; i < instrument.size(); ++i)
        {
            const char c = instrument[i];
            if (c < '0' || c > '9') return -1;
            v = v * 10 + (c - '0');
            if (v > 127) return -1;
        }
        return v;
    }

    std::string instrument (int p) { return "gm:" + std::to_string (std::clamp (p, 0, 127)); }

    const char* nomFamilia (int f)
    {
        static const char* const noms[16] = {
            "Pianos", "Percussió afinada", "Orgues", "Guitarres", "Baixos", "Cordes", "Conjunts", "Metalls",
            "Llengüetes", "Flautes", "Sintes solistes", "Sintes de fons", "Efectes de sinte", "Ètnics",
            "Percussió", "Efectes de so" };
        return noms[std::clamp (f, 0, 15)];
    }

    const char* nomPrograma (int p)
    {
        static const char* const noms[128] = {
            // Pianos
            "Piano de cua", "Piano brillant", "Piano elèctric de cua", "Piano de taverna",
            "Piano elèctric 1 (Rhodes)", "Piano elèctric 2 (DX)", "Clavecí", "Clavinet",
            // Percussió afinada
            "Celesta", "Glockenspiel", "Caixa de música", "Vibràfon", "Marimba", "Xilòfon", "Campanes tubulars", "Dulcimer",
            // Orgues
            "Orgue Hammond", "Orgue percussiu", "Orgue de rock", "Orgue d'església", "Harmònium", "Acordió", "Harmònica", "Bandoneó",
            // Guitarres
            "Guitarra clàssica (niló)", "Guitarra acústica (acer)", "Guitarra elèctrica jazz", "Guitarra elèctrica neta",
            "Guitarra elèctrica apagada", "Guitarra saturada", "Guitarra distorsionada", "Harmònics de guitarra",
            // Baixos
            "Contrabaix acústic", "Baix elèctric (dits)", "Baix elèctric (pua)", "Baix sense trasts",
            "Baix slap 1", "Baix slap 2", "Baix de sinte 1", "Baix de sinte 2",
            // Cordes
            "Violí", "Viola", "Violoncel", "Contrabaix", "Cordes en trèmolo", "Cordes en pizzicato", "Arpa", "Timbales",
            // Conjunts
            "Conjunt de cordes 1", "Conjunt de cordes 2", "Cordes de sinte 1", "Cordes de sinte 2",
            "Cor «aah»", "Veu «ooh»", "Veu de sinte", "Cop d'orquestra",
            // Metalls
            "Trompeta", "Trombó", "Tuba", "Trompeta amb sordina", "Trompa", "Secció de metalls", "Metalls de sinte 1", "Metalls de sinte 2",
            // Llengüetes
            "Saxo soprano", "Saxo alt", "Saxo tenor", "Saxo baríton", "Oboè", "Corn anglès", "Fagot", "Clarinet",
            // Flautes
            "Flautí", "Flauta travessera", "Flauta dolça", "Flauta de Pan", "Ampolla bufada", "Shakuhachi", "Xiulet", "Ocarina",
            // Sintes solistes
            "Solista quadrada", "Solista de serra", "Solista calliope", "Solista chiff",
            "Solista charang", "Solista de veu", "Solista de quintes", "Solista baix + solista",
            // Sintes de fons
            "Fons «new age»", "Fons càlid", "Fons polisinte", "Fons de cor", "Fons d'arc", "Fons metàl·lic", "Fons halo", "Fons d'escombrat",
            // Efectes de sinte
            "Pluja", "Banda sonora", "Cristall", "Atmosfera", "Brillantor", "Follets", "Ecos", "Ciència-ficció",
            // Ètnics
            "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Gaita", "Violí folk", "Shanai",
            // Percussió
            "Campaneta", "Agogô", "Steel drums", "Bloc de fusta", "Taiko", "Tom melòdic", "Tambor de sinte", "Plat al revés",
            // Efectes de so
            "Soroll de trasts", "Respiració", "Onades", "Ocells", "Telèfon", "Helicòpter", "Aplaudiments", "Tret" };
        return noms[std::clamp (p, 0, 127)];
    }

    // ---- SinteGM ----

    SinteGM::SinteGM (std::shared_ptr<Banc> b, int p) : banc (std::move (b)), prog (std::clamp (p, 0, 127))
    {
        if (banc != nullptr && banc->f != nullptr)
            sinte = tsf_copy (banc->f);
    }

    SinteGM::~SinteGM()
    {
        if (sinte != nullptr) tsf_close (static_cast<tsf*> (sinte));
    }

    void SinteGM::prepara (double fs, int bloc)
    {
        maxBloc = std::max (1, bloc);
        entrellacat.assign ((size_t) maxBloc * 2, 0.0f);
        auto* f = static_cast<tsf*> (sinte);
        if (f == nullptr) return;
        tsf_reset (f);
        tsf_set_output (f, TSF_STEREO_INTERLEAVED, (int) (fs + 0.5), -6.0f);
        tsf_set_max_voices (f, 64);                 // reserva les veus ara: tocar no demana memòria
        tsf_channel_set_presetnumber (f, 0, prog);  // reserva el canal
        tsf_channel_set_bank_preset (f, 0, 0, prog);
    }

    void SinteGM::renderitza (float* L, float* R, int ini, int fi) noexcept
    {
        auto* f = static_cast<tsf*> (sinte);
        while (ini < fi)
        {
            const int n = std::min (fi - ini, maxBloc);
            tsf_render_float (f, entrellacat.data(), n, 0);
            for (int i = 0; i < n; ++i)
            {
                L[ini + i] = entrellacat[(size_t) (2 * i)];
                R[ini + i] = entrellacat[(size_t) (2 * i + 1)];
            }
            ini += n;
        }
    }

    void SinteGM::processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept
    {
        auto* f = static_cast<tsf*> (sinte);
        if (f == nullptr || entrellacat.empty())
        {
            std::fill (L, L + n, 0.0f);
            std::fill (R, R + n, 0.0f);
            return;
        }
        int pos = 0;
        for (int e = 0; e < numEvents; ++e)
        {
            const auto& ev = events[e];
            const int off = std::clamp (ev.offset, 0, n);
            if (off > pos) { renderitza (L, R, pos, off); pos = off; }
            const int tipus = ev.estat & 0xF0;
            if (tipus == 0x90 && ev.dada2 > 0) tsf_channel_note_on (f, 0, ev.dada1, ev.dada2 / 127.0f);
            else if (tipus == 0x80 || tipus == 0x90) tsf_channel_note_off (f, 0, ev.dada1);
            else if (tipus == 0xB0)
            {
                if (ev.dada1 == 0 || ev.dada1 == 32) continue;   // canvis de banc: no
                tsf_channel_midi_control (f, 0, ev.dada1, ev.dada2);
            }
            else if (tipus == 0xE0) tsf_channel_set_pitchwheel (f, 0, ev.dada1 | (ev.dada2 << 7));
        }
        if (pos < n) renderitza (L, R, pos, n);
    }

    void SinteGM::apagaTot() noexcept
    {
        if (auto* f = static_cast<tsf*> (sinte)) tsf_note_off_all (f);
    }

    bool SinteGM::fentSo() const noexcept
    {
        auto* f = static_cast<tsf*> (sinte);
        return f != nullptr && tsf_active_voice_count (f) > 0;
    }
}

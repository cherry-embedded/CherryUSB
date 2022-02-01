/**
 * @file
 * @brief USB MIDI Class public header
 *
 */

#ifndef _USB_MIDI_H_
#define _USB_MIDI_H_

/* MIDI Streaming class specific interfaces */
#define MIDI_IN_JACK  0x02
#define MIDI_OUT_JACK 0x03

#define MIDI_JACK_EMBEDDED 0x01
#define MIDI_JACK_EXTERNAL 0x02

#define MIDI_CHANNEL_OMNI 0
#define MIDI_CHANNEL_OFF  17

#define MIDI_PITCHBEND_MIN -8192
#define MIDI_PITCHBEND_MAX 8191

/*! Enumeration of MIDI types */
enum MidiType {
    InvalidType = 0x00,          ///< For notifying errors
    NoteOff = 0x80,              ///< Note Off
    NoteOn = 0x90,               ///< Note On
    AfterTouchPoly = 0xA0,       ///< Polyphonic AfterTouch
    ControlChange = 0xB0,        ///< Control Change / Channel Mode
    ProgramChange = 0xC0,        ///< Program Change
    AfterTouchChannel = 0xD0,    ///< Channel (monophonic) AfterTouch
    PitchBend = 0xE0,            ///< Pitch Bend
    SystemExclusive = 0xF0,      ///< System Exclusive
    TimeCodeQuarterFrame = 0xF1, ///< System Common - MIDI Time Code Quarter Frame
    SongPosition = 0xF2,         ///< System Common - Song Position Pointer
    SongSelect = 0xF3,           ///< System Common - Song Select
    TuneRequest = 0xF6,          ///< System Common - Tune Request
    Clock = 0xF8,                ///< System Real Time - Timing Clock
    Start = 0xFA,                ///< System Real Time - Start
    Continue = 0xFB,             ///< System Real Time - Continue
    Stop = 0xFC,                 ///< System Real Time - Stop
    ActiveSensing = 0xFE,        ///< System Real Time - Active Sensing
    SystemReset = 0xFF,          ///< System Real Time - System Reset
};

/*! Enumeration of Thru filter modes */
enum MidiFilterMode {
    Off = 0,              ///< Thru disabled (nothing passes through).
    Full = 1,             ///< Fully enabled Thru (every incoming message is sent back).
    SameChannel = 2,      ///< Only the messages on the Input Channel will be sent back.
    DifferentChannel = 3, ///< All the messages but the ones on the Input Channel will be sent back.
};

/*! \brief Enumeration of Control Change command numbers.
 See the detailed controllers numbers & description here:
 http://www.somascape.org/midi/tech/spec.html#ctrlnums
 */
enum MidiControlChangeNumber {
    // High resolution Continuous Controllers MSB (+32 for LSB) ----------------
    BankSelect = 0,
    ModulationWheel = 1,
    BreathController = 2,
    // CC3 undefined
    FootController = 4,
    PortamentoTime = 5,
    DataEntry = 6,
    ChannelVolume = 7,
    Balance = 8,
    // CC9 undefined
    Pan = 10,
    ExpressionController = 11,
    EffectControl1 = 12,
    EffectControl2 = 13,
    // CC14 undefined
    // CC15 undefined
    GeneralPurposeController1 = 16,
    GeneralPurposeController2 = 17,
    GeneralPurposeController3 = 18,
    GeneralPurposeController4 = 19,

    // Switches ----------------------------------------------------------------
    Sustain = 64,
    Portamento = 65,
    Sostenuto = 66,
    SoftPedal = 67,
    Legato = 68,
    Hold = 69,

    // Low resolution continuous controllers -----------------------------------
    SoundController1 = 70,  ///< Synth: Sound Variation   FX: Exciter On/Off
    SoundController2 = 71,  ///< Synth: Harmonic Content  FX: Compressor On/Off
    SoundController3 = 72,  ///< Synth: Release Time      FX: Distortion On/Off
    SoundController4 = 73,  ///< Synth: Attack Time       FX: EQ On/Off
    SoundController5 = 74,  ///< Synth: Brightness        FX: Expander On/Off
    SoundController6 = 75,  ///< Synth: Decay Time        FX: Reverb On/Off
    SoundController7 = 76,  ///< Synth: Vibrato Rate      FX: Delay On/Off
    SoundController8 = 77,  ///< Synth: Vibrato Depth     FX: Pitch Transpose On/Off
    SoundController9 = 78,  ///< Synth: Vibrato Delay     FX: Flange/Chorus On/Off
    SoundController10 = 79, ///< Synth: Undefined         FX: Special Effects On/Off
    GeneralPurposeController5 = 80,
    GeneralPurposeController6 = 81,
    GeneralPurposeController7 = 82,
    GeneralPurposeController8 = 83,
    PortamentoControl = 84,
    // CC85 to CC90 undefined
    Effects1 = 91, ///< Reverb send level
    Effects2 = 92, ///< Tremolo depth
    Effects3 = 93, ///< Chorus send level
    Effects4 = 94, ///< Celeste depth
    Effects5 = 95, ///< Phaser depth

    // Channel Mode messages ---------------------------------------------------
    AllSoundOff = 120,
    ResetAllControllers = 121,
    LocalControl = 122,
    AllNotesOff = 123,
    OmniModeOff = 124,
    OmniModeOn = 125,
    MonoModeOn = 126,
    PolyModeOn = 127
};

struct midi_cs_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t SubType;
    uint16_t bcdADC;
    uint16_t wTotalLength;
} __PACKED;

struct midi_in_jack_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t SubType;
    uint8_t bJackType;
    uint8_t bJackId;
    uint8_t iJack;
} __PACKED;

struct midi_out_jack_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t SubType;
    uint8_t bJackType;
    uint8_t bJackId;
    uint8_t bNrInputPins;
    uint8_t baSourceId;
    uint8_t baSourcePin;
    uint8_t iJack;
} __PACKED;

#define MIDI_ADAPTER_AC_INTERFACE_DESCRIPTOR_SIZE(num) (8 + num)
#define MIDI_ADAPTER_AC_INTERFACE_DESCRIPTOR(num) \
    struct midi_adapter_ac_interface_descriptor { \
        uint8_t bLength;                          \
        uint8_t bDescriptorType;                  \
        uint8_t SubType;                          \
        uint16_t bcdADC;                          \
        uint16_t wTotalLength;                    \
        uint8_t bInCollection;                    \
        uint8_t baInterfaceNr[num];               \
    } __PACKED

#define MIDI_CS_BULK_ENDPOINT_DESCRIPTOR_SIZE(num) (4 + num)
#define MIDI_CS_BULK_ENDPOINT_DESCRIPTOR(num) \
    struct midi_cs_bulk_descriptor {          \
        uint8_t bLength;                      \
        uint8_t bDescriptorType;              \
        uint8_t SubType;                      \
        uint8_t bNumEmbMIDIJack;              \
        uint8_t baAssocJackID[num];           \
    } __PACKED

#endif /* _USB_MIDI_H_ */

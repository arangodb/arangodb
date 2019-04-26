interface AU_Color {
    rgb: number[];
    class_name: string;
}
interface TextWithAttr {
    fg: AU_Color;
    bg: AU_Color;
    bold: boolean;
    text: string;
}
declare enum PacketKind {
    EOS = 0,
    Text = 1,
    Incomplete = 2,
    ESC = 3,
    Unknown = 4,
    SGR = 5,
    OSCURL = 6
}
interface TextPacket {
    kind: PacketKind;
    text: string;
    url: string;
}
export declare class AnsiUp {
    VERSION: string;
    private ansi_colors;
    private palette_256;
    private fg;
    private bg;
    private bold;
    private _use_classes;
    private _escape_for_html;
    private _csi_regex;
    private _osc_st;
    private _osc_regex;
    private _url_whitelist;
    private _buffer;
    constructor();
    use_classes: boolean;
    escape_for_html: boolean;
    url_whitelist: {};
    private setup_palettes;
    private escape_txt_for_html;
    append_buffer(txt: string): void;
    get_next_packet(): TextPacket;
    ansi_to_html(txt: string): string;
    private with_state;
    private process_ansi;
    transform_to_html(fragment: TextWithAttr): string;
    private process_hyperlink;
}
export {};

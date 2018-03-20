interface AU_Color {
    rgb: number[];
    class_name: string;
}
interface TextWithData {
    fg: AU_Color;
    bg: AU_Color;
    bright: boolean;
    text: string;
}
interface Formatter {
    transform(fragment: TextWithData, instance: AnsiUp): any;
    compose(segments: any[], instance: AnsiUp): any;
}
declare function rgx(tmplObj: any, ...subst: any[]): RegExp;
declare class AnsiUp {
    VERSION: string;
    ansi_colors: {
        rgb: number[];
        class_name: string;
    }[][];
    htmlFormatter: Formatter;
    textFormatter: Formatter;
    private palette_256;
    private fg;
    private bg;
    private bright;
    private _use_classes;
    private _escape_for_html;
    private _sgr_regex;
    private _buffer;
    constructor();
    use_classes: boolean;
    escape_for_html: boolean;
    private setup_256_palette();
    private old_escape_for_html(txt);
    private old_linkify(txt);
    private detect_incomplete_ansi(txt);
    private detect_incomplete_link(txt);
    ansi_to(txt: string, formatter: Formatter): any;
    ansi_to_html(txt: string): string;
    ansi_to_text(txt: string): string;
    private with_state(text);
    private handle_incomplete_sequences(chunks);
    private process_ansi(block);
}

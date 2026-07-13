#pragma once
#include <JuceHeader.h>

// Tiny Markdown -> juce::AttributedString renderer for the online-help panels (Story 6.1).
// The help texts moved from flat JSON to per-module Markdown files precisely so they can carry
// formatting; this is the "compact" subset that renders it:
//
//   • paragraphs           (blank line separates blocks)
//   • headings             # h1, ## / ### h2   (larger + bold)
//   • bullet lists         lines starting with "- " or "* "  -> "•  " prefix
//   • inline **bold**, *italic*, `code`
//
// It is intentionally NOT a full CommonMark parser: no tables, nested lists, links or images.
// Everything is emitted into a single AttributedString so HelpPanel can measure (TextLayout) and
// paint it consistently at one content width. Wrapped bullet lines do not hang-indent — fine for
// the short texts here.
namespace md
{
    struct Style
    {
        float base        = 14.0f;
        float heading1    = 17.0f;
        float heading2    = 15.5f;
        float lineSpacing = 2.0f;
        juce::Colour text    { 0xffcdd3dc };
        juce::Colour heading { 0xffe8edf3 };
        juce::Colour code    { 0xff9fd6cf };
    };

    // Append `text`, resolving **bold** / *italic* / `code`. `baseStyle` is the style already
    // active for the whole segment (e.g. headings pass Font::bold), which the inline markers
    // then toggle on top of.
    inline void appendInline (juce::AttributedString& out, const juce::String& text,
                              float size, int baseStyle, const Style& st)
    {
        bool bold   = (baseStyle & juce::Font::bold)   != 0;
        bool italic = (baseStyle & juce::Font::italic) != 0;
        bool code   = false;
        juce::String buf;

        auto flush = [&]
        {
            if (buf.isEmpty())
                return;
            const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
            if (code)
                out.append (buf, juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                               size, style)), st.code);
            else
                out.append (buf, juce::Font (juce::FontOptions (size, style)), st.text);
            buf.clear();
        };

        const int n = text.length();
        for (int i = 0; i < n; )
        {
            const juce::juce_wchar c    = text[i];
            const juce::juce_wchar next = (i + 1 < n) ? text[i + 1] : 0;

            if (! code && c == '*' && next == '*') { flush(); bold   = ! bold;   i += 2; continue; }
            if (! code && c == '*')                { flush(); italic = ! italic; i += 1; continue; }
            if (c == '`')                          { flush(); code   = ! code;   i += 1; continue; }

            buf += c;
            ++i;
        }
        flush();
    }

    inline juce::AttributedString render (const juce::String& markdown, const Style& st = {})
    {
        juce::AttributedString out;
        out.setLineSpacing (st.lineSpacing);

        const auto lines = juce::StringArray::fromLines (markdown);
        bool first      = true;   // no leading newline before the very first block
        bool pendingGap = false;  // a blank source line requests extra space before the next block

        // Start a new visual block: nothing before the first, else a single or (for a paragraph
        // break) double newline. Emitted in the base font so the gap height is predictable.
        auto newBlock = [&] (bool gap)
        {
            if (first) { first = false; return; }
            out.append (gap ? "\n\n" : "\n", juce::Font (juce::FontOptions (st.base)), st.text);
        };

        for (const auto& raw : lines)
        {
            const auto line = raw.trimEnd();
            if (line.trim().isEmpty()) { pendingGap = true; continue; }

            // heading: leading run of '#'
            if (line.startsWith ("# ") || line.startsWith ("## ") || line.startsWith ("### "))
            {
                const auto hashes  = line.initialSectionContainingOnly ("#");
                const auto content = line.substring (hashes.length()).trim();
                const float sz     = (hashes.length() <= 1) ? st.heading1 : st.heading2;
                newBlock (true);   // headings always break with a gap
                appendInline (out, content, sz, juce::Font::bold, st);
                pendingGap = false;
                continue;
            }

            // bullet
            const auto t = line.trimStart();
            if (t.startsWith ("- ") || t.startsWith ("* "))
            {
                newBlock (pendingGap);
                out.append (juce::String::fromUTF8 ("\xE2\x80\xA2  "),   // "•  "
                            juce::Font (juce::FontOptions (st.base)), st.text);
                appendInline (out, t.substring (2).trim(), st.base, 0, st);
                pendingGap = false;
                continue;
            }

            // normal paragraph line
            newBlock (pendingGap);
            appendInline (out, line, st.base, 0, st);
            pendingGap = false;
        }

        return out;
    }
}

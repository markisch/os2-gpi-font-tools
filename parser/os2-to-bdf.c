
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "otypes.h"
#include "gpifont.h"
#include "pmugl.h"

void write_glyph( ULONG ulOffset, FILE * f, POS2FONTRESOURCE pFont )
{
    GLYPHBITMAP glyph;
    USHORT      i, j;

    if ( ! ExtractOS2FontGlyph( ulOffset, pFont, &glyph )) {
        printf("Failed to extract glyph bitmap.\n");
        return;
    }

    // FIXME: Should we skip glyphs which are identical to the default char?

    // glyph header
    if (ulOffset <= OS2UGL_MAX_GLYPH && (ulOffset == 0 || UGL2Uni[ulOffset] != 0)) {
        fprintf(f, "STARTCHAR U+%04X\n", UGL2Uni[ulOffset]);
    } else {
        // No Unicode number available: glyph index out of range or value == 0
        fprintf(f, "STARTCHAR glyph%u\n", ulOffset);
    }
    // glyph encoding
    if (pFont->pMetrics->usCodePage == 850)  { // UGL encoding
        if (ulOffset <= OS2UGL_MAX_GLYPH && (ulOffset == 0 || UGL2Uni[ulOffset] != 0))
            fprintf(f, "ENCODING %u\n", UGL2Uni[ulOffset]);  // recode to Unicode
        else
            fprintf(f, "ENCODING %i\n", -1);  // do not encode
    } else {
        fprintf(f, "ENCODING %u\n", ulOffset);  // Symbol font, or other codepage
    }

    // FIXME: gbdfed corrects a number of glyphs where the width is incorrect
    fprintf(f, "SWIDTH %u %u\n", glyph.horiAdvance * 72 / pFont->pMetrics->xDeviceRes * 10000 / pFont->pMetrics->usNominalPointSize, 0);
    fprintf(f, "DWIDTH %u %u\n", glyph.horiAdvance, 0);
    fprintf(f, "BBX %u %u %u %i\n", glyph.width, glyph.rows, glyph.horiBearingX, pFont->pFontDef->pCellBaseOffset - glyph.rows);

    // glyph bitmap
    // FIXME: The standard covers only "trimmed" glyphs. Should we do that?
    //        All tested tools tried accept our output, though.
    fprintf(f, "BITMAP\n");
    for ( i = 0; i < glyph.rows; i++ ) {
        for ( j = 0; j < glyph.pitch; j++ ) {
            fprintf(f, "%02X", glyph.buffer[ (i * glyph.pitch) + j ]);
        }
        fprintf(f, "\n");
    }

    // glyph footer
    fprintf(f, "ENDCHAR\n");

    free( glyph.buffer );
}


int main( int argc, char *argv[] )
{
    PSZ             pszFile;
    OS2FONTRESOURCE font = {0};
    ULONG           index,
                    total,
                    offset,
                    error;
    // GLYPHBITMAP     glyph;
    char            BDFfilename[512];

    if ( argc < 2 ) {
        printf("Syntax: os2-to-bdf <FNT filename> [file index]\n");
        return 0;
    }
    pszFile = argv[1];
    if (( argc < 3 ) || ( !sscanf( argv[2], "%x", &index )))
        index = 0;

    do {

        error = ReadOS2FontResource( pszFile, index, &total, &font );
        if ( error ) {
            switch ( error ) {
                case ERR_FILE_OPEN:
                    printf("The file %s could not be opened.\n", pszFile );
                    break;
                case ERR_FILE_STAT:
                case ERR_FILE_READ:
                    printf("Failed to read file %s.\n", pszFile );
                    break;
                case ERR_FILE_FORMAT:
                    printf("The file %s does not contain a valid font.\n", pszFile );
                    break;
                case ERR_NO_FONT:
                    printf("The requested font number was not found in %s\n", pszFile );
                    break;
                case ERR_MEMORY:
                    printf("A memory allocation error occurred.\n");
                    break;
                default:
                    printf("An unknown error occurred.\n");
                    break;
            }
            return error;
        }

        printf("File %s contains %u fonts.  Reading font %i.\n", pszFile, total, index );
        printf(" - Font signature:    %s\n", font.pSignature->achSignature );
        printf(" - Family name:       %s\n - Face name:         %s\n",
               font.pMetrics->szFamilyname, font.pMetrics->szFacename );
        if ( font.pMetrics->usKerningPairs )
            printf(" - %d kerning pairs are defined\n", font.pMetrics->usKerningPairs );
        if ( font.pPanose )
            printf(" - PANOSE table:      (%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)\n",
                   font.pPanose->panose[0], font.pPanose->panose[1],
                   font.pPanose->panose[2], font.pPanose->panose[3],
                   font.pPanose->panose[4], font.pPanose->panose[5],
                   font.pPanose->panose[6], font.pPanose->panose[7],
                   font.pPanose->panose[8], font.pPanose->panose[8] );
        printf(" - Font type:         ");
        switch ( font.pFontDef->fsChardef ) {
            case OS2FONTDEF_CHAR3:
                printf("3 (ABC-space proportional-width)\n");
                break;
            default:
                if ( font.pFontDef->fsFontdef == OS2FONTDEF_FONT2 )
                    printf("2 (single-increment proportional-width)\n");
                else
                    printf("1 (fixed-width)\n");
                break;
        }

        printf(" - Point size:        %u (%ux%u dpi)\n",
               font.pMetrics->usNominalPointSize / 10,
               font.pMetrics->xDeviceRes, font.pMetrics->yDeviceRes );
        printf(" - First glyph index: %u\n", font.pMetrics->usFirstChar );
        printf(" - Last glyph index:  %u\n", font.pMetrics->usLastChar + font.pMetrics->usFirstChar );
        printf(" - Default character: %u\n", font.pMetrics->usDefaultChar + font.pMetrics->usFirstChar );
        printf(" - Break character:   %u\n", font.pMetrics->usBreakChar + font.pMetrics->usFirstChar );

        printf("\n");

        // open BDF file
        // sprintf(BDFfilename, "%s-%u.bdf", pszFile, index);
        sprintf(BDFfilename, "%s-%u-%u-%u.bdf",
            font.pMetrics->szFacename,
            font.pMetrics->usNominalPointSize / 10,
            font.pMetrics->yMaxBaselineExt,
            font.pMetrics->xDeviceRes);
        FILE * f = fopen(BDFfilename, "wb");  // we only want LF line endings
        if (!f) {
            fprintf(stderr, "Could not open file %s for writing.\n", BDFfilename);
            return 1;
        }
        printf("Save BDF data to \"%s\"\n\n", BDFfilename);

        // Write BDF header
        fprintf(f, "STARTFONT 2.1\n");
        fprintf(f, "COMMENT Converted from an OS/2 bitmap font using os2-to-bdf.\n");

        // build XLFD like font string
        const char foundry[] = "Misc";
        fprintf(f, "FONT -%s", foundry); // FOUNDRY
        fprintf(f, "-%s", font.pMetrics->szFamilyname); // FAMILY_NAME
        const char weight[10][12] = {
            "None",
            "UltraLight", "ExtraLight", "Light", "SemiLight", "Medium",
            "SemiBold", "Bold", "ExtraBold", "UltraBold"
        };
        fprintf(f, "-%s", weight[font.pMetrics->usWeightClass]); // WEIGHT_NAME
        char slant;
        if (font.pMetrics->fsSelectionFlags & FM_SEL_ITALIC) // could also test
            slant = 'I';
        else
            slant = 'R';
        fprintf(f, "-%c", slant); // SLANT
        const char width[10][20] = {
            "None",
            "UltraCondensed", "ExtraCondensed", "Condensed", "SemiCondensed",
            "Medium", "SemiExpanded", "ExtraExpanded", "UltraExpanded"
        };
        fprintf(f, "-%s", width[font.pMetrics->usWidthClass]); // SETWIDTH_NAME
        fprintf(f, "-"); // ADD_STYLE_NAME
        fprintf(f, "-%u", font.pMetrics->yMaxBaselineExt); // PIXEL_SIZE
        fprintf(f, "-%u", font.pMetrics->usNominalPointSize); // POINT_SIZE
        fprintf(f, "-%u", font.pMetrics->xDeviceRes); // RESOLUTION_X
        fprintf(f, "-%u", font.pMetrics->yDeviceRes); // RESOLUTION_Y
        char spacing;
        if (font.pMetrics->fsTypeFlags & FM_TYPE_FIXED)
            spacing = 'M';
        else
            spacing = 'P';
        fprintf(f, "-%c", spacing); // SPACING  // FIXME: CHARCELL?
        fprintf(f, "-%u", font.pMetrics->xAveCharWidth * 10); // AVERAGE_WIDTH
        // CHARSET_REGISTRY-CHARSET_ENCODING
        char registry[128];
        char encoding[128];
        if (font.pMetrics->usCodePage == 850) {
            // UGL fonts are translated to Unicode
            strcpy(registry, "ISO10646");
            strcpy(encoding, "1");
        } else if (font.pMetrics->usCodePage == 65400) {
            // Symbol font with special encoding
            strcpy(registry, "adobe");
            strcpy(encoding, "fontspecific");
        } else {
            // FIXME: wild guess here, not tested
            strcpy(registry, "ibm");
            sprintf(encoding, "cp%u", font.pMetrics->usCodePage);
        }
        fprintf(f, "-%s-%s\n", registry, encoding);

        // fprintf(f, "FONT %s\n", font.pMetrics->szFacename);
        fprintf(f, "SIZE %u %u %u\n", font.pMetrics->usNominalPointSize/10,
                   font.pMetrics->xDeviceRes, font.pMetrics->yDeviceRes );
        // Not sure what the correct way to determine the font bounding box is,
        // but this should be the largest allowed bounding box.
        fprintf(f, "FONTBOUNDINGBOX %u %u %i %i\n", font.pMetrics->xMaxCharInc, font.pMetrics->yMaxBaselineExt, 0, -font.pMetrics->yMaxDescender );
        // Not sure what the correct way to determine the font bounding box is,
        // so we just use that of the 'A' glyph.
        // if ( ExtractOS2FontGlyph( OS2FontGlyphIndex( &font, 65 ), &font, &glyph )) {
            // fprintf(f, "FONTBOUNDINGBOX %u %u %i %i\n", glyph.width, glyph.rows, glyph.horiBearingX, font.pFontDef->pCellBaseOffset - glyph.rows );
        // }
        // free( glyph.buffer );

        // properties
        fprintf(f, "STARTPROPERTIES 18\n");
        //
        // FIXME:  The copyright is typically somewhere in the font file
        fprintf(f, "COPYRIGHT \"%s %s\"\n", "For copyright see the original file:", pszFile);
        fprintf(f, "FONT_ASCENT %u\n", font.pMetrics->yMaxAscender);
        fprintf(f, "FONT_DESCENT %u\n", font.pMetrics->yMaxDescender);
        if (font.pMetrics->usCodePage == 850)
            fprintf(f, "DEFAULT_CHAR %u\n", UGL2Uni[font.pMetrics->usDefaultChar + font.pMetrics->usFirstChar]);
        else
            fprintf(f, "DEFAULT_CHAR %u\n", font.pMetrics->usDefaultChar + font.pMetrics->usFirstChar);
        // These actually repeat the XLFD font name above
        fprintf(f, "FOUNDRY \"%s\"\n", foundry);
        fprintf(f, "FAMILY_NAME \"%s\"\n", font.pMetrics->szFamilyname);
        fprintf(f, "WEIGHT_NAME \"%s\"\n", weight[font.pMetrics->usWeightClass]);
        fprintf(f, "SLANT \"%c\"\n", slant);
        fprintf(f, "SETWIDTH_NAME \"%s\"\n", width[font.pMetrics->usWidthClass]);
        fprintf(f, "ADD_STYLE_NAME \"%s\"\n", "");
        fprintf(f, "PIXEL_SIZE %u\n", font.pMetrics->yMaxBaselineExt);
        fprintf(f, "POINT_SIZE %u\n", font.pMetrics->usNominalPointSize);
        fprintf(f, "RESOLUTION_X %u\n", font.pMetrics->xDeviceRes);
        fprintf(f, "RESOLUTION_Y %u\n", font.pMetrics->yDeviceRes);
        fprintf(f, "SPACING \"%c\"\n", spacing);
        fprintf(f, "AVERAGE_WIDTH %u\n", font.pMetrics->xAveCharWidth * 10);
        // parse var encoding registry "-" enc
        fprintf(f, "CHARSET_REGISTRY \"%s\"\n", registry);
        fprintf(f, "CHARSET_ENCODING \"%s\"\n", encoding);
        //
        fprintf(f, "ENDPROPERTIES\n");

        // start glyph section
        fprintf(f, "CHARS %u\n", font.pMetrics->usLastChar + 1);

        /* qsort comparison function */
        int mapping_cmp(const void *a, const void *b)
        {
            // we sort by the second element of every row
            const int *ia = (const int *)a + 1;
            const int *ib = (const int *)b + 1;
            // integer comparison: returns negative if b > a and positive if a > b
            return *ia  - *ib;
        }

        // in order to remap UGL to unicode, we have to create a sorted
        // mapping list?!
        if (font.pMetrics->usCodePage == 850) {
            int mapping[OS2UGL_MAX_GLYPH + 1][2];
            int i, maxi;

            if (font.pMetrics->usLastChar > OS2UGL_MAX_GLYPH)
                maxi = OS2UGL_MAX_GLYPH;
            else
                maxi = font.pMetrics->usLastChar;

            // make a local copy of the table with both values
            mapping[0][0] = mapping[0][1] = 0;
            for (i = 1; i <= maxi; i++) {
                mapping[i][0] = i;
                // For glyphs without Unicode equivalent we add an offset
                // to ensure they are sorted at the end of the list.
                // They will be left "unencoded".
                if (UGL2Uni[i] != 0)
                    mapping[i][1] = UGL2Uni[i];
                else
                    mapping[i][1] = i + 0x10000;
            }

            // now sort the mapping table
            qsort(mapping, maxi + 1, 2*sizeof(int), mapping_cmp);

            // Iterate over all glyph indices according to the mapping
            for (i = 0; i <= font.pMetrics->usLastChar; i++) {
                if (i <=  OS2UGL_MAX_GLYPH)
                    offset = mapping[i][0];
                else
                    offset = i;
                // printf("%i: offset = %i\n", i, offset);
                offset += font.pMetrics->usFirstChar;
                write_glyph(offset, f, &font);
            }

        } else {

            // Simply iterate over all glyph indices; dump one at a time
            for (offset = font.pMetrics->usFirstChar; offset < font.pMetrics->usLastChar + font.pMetrics->usFirstChar; offset++) {
                // dump current glyph
                write_glyph(offset, f, &font);
            }

        }

        // Write BDF footer
        fprintf(f, "ENDFONT\n");

        fclose(f);

    } while (++index < total);

    printf("DONE.\n");

    return 0;
}

"""The games Tera Recomp extracts, for the vendored MacNativeGames extractors (tools/vendor/macnativegames).

Those scripts take only TotK's title ID in extract_exefs.program_nca, and tools/vendor/ stays byte-identical to
upstream. install() replaces that function in the imported module with program_nca below, which takes the base game and
the update of Pokemon Scarlet or Violet (and TotK). extract_romfs.py and extract_update_romfs.py import program_nca from
that module, so they see the replacement once it is installed; extract.py runs extract_exefs.py through its main()
for the same reason (extract.RUN_SCRIPT).
"""
# application title ID -> name. An update's Meta NCA carries the application ID with bit 11 set (...800); its Program
# NCA keeps the application ID.
GAMES = {'0100A3D008C5C000': 'Pokemon Scarlet', '01008F6008C5E000': 'Pokemon Violet',
         '0100F2C0115B6000': 'The Legend of Zelda: Tears of the Kingdom'}


def update_title(title):
    return f'{int(title, 16) | 0x800:016X}'


def program_nca(records, update=False):
    """extract_exefs.program_nca with the titles of GAMES instead of TotK's alone; the same checks otherwise."""
    from extract_exefs import require
    if update:
        programs = [(r, h) for r, h in records if r['content_type'] == 0]
        require(len(programs) == 1, 'Expected one update Program NCA; combined packages are unsupported')
        record, hdr = programs[0]
        title = record['title_id']
        require(title in GAMES, 'Update belongs to another game')
        require(any(hdr[0x400 + i * 512 + 4] == 4 for i in range(4)), 'Expected a BKTR update, not a base game')
        require(all(r['title_id'] in (title, update_title(title)) for r, _ in records), 'Mixed-title update package')
        return record, hdr
    metadata = [r for r, _ in records if r['content_type'] == 1]
    require(len(metadata) <= 1 and all(r['title_id'] in GAMES for r in metadata),
            'Expected base game metadata; updates, DLC and combined packages are unsupported')
    programs = [(r, h) for r, h in records if r['content_type'] == 0]
    require(len(programs) == 1, 'Expected one base Program NCA; updates, DLC and combined packages are unsupported')
    record, hdr = programs[0]
    require(record['title_id'] in GAMES,
            'Expected the base game of Pokemon Scarlet or Violet; updates, DLC and other games are unsupported')
    return record, hdr


def install():
    import extract_exefs
    extract_exefs.program_nca = program_nca

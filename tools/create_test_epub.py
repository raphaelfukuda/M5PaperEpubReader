#!/usr/bin/env python3
"""Generate copyright-free valid and invalid EPUB fixtures."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile

ROOT = Path(__file__).resolve().parents[1] / "test" / "fixtures"
MIMETYPE = b"application/epub+zip"
CONTAINER = b'''<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
 <rootfiles><rootfile full-path="OPS/package.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>'''
OPF = '''<?xml version="1.0" encoding="UTF-8"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf" unique-identifier="id">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:identifier id="id">urn:uuid:m5paper-test</dc:identifier>
  <dc:title>Livro de Teste em Português</dc:title><dc:creator>Projeto M5Paper</dc:creator><dc:language>pt-BR</dc:language>
 </metadata>
 <manifest>
  <item id="c1" href="text/chapter1.xhtml" media-type="application/xhtml+xml"/>
  <item id="c2" href="chapters/chapter2.xhtml" media-type="application/xhtml+xml"/>
 </manifest>
 <spine><itemref idref="c1"/><itemref idref="c2"/></spine>
</package>'''
CH1 = '''<html xmlns="http://www.w3.org/1999/xhtml"><body><h1>Capítulo um</h1>
<p>Água, ação, coração, informação e português.</p><p><strong>Negrito</strong>, <em>itálico</em> e &amp; entidade.</p>
<ul><li>Primeiro item</li><li>Segundo item</li></ul>
<p>palavramuitolongapalavramuitolongapalavramuitolongapalavramuitolonga</p></body></html>'''
CH2 = '''<html xmlns="http://www.w3.org/1999/xhtml"><body><h2>Segundo capítulo</h2>
<p>Este capítulo fica em outro subdiretório e valida caminhos relativos.</p></body></html>'''

def write_epub(path: Path, container=CONTAINER, opf=OPF, include_c2=True, traversal=False):
    with ZipFile(path, "w") as z:
        z.writestr("mimetype", MIMETYPE, ZIP_STORED)
        if container is not None: z.writestr("META-INF/container.xml", container, ZIP_DEFLATED)
        if opf is not None: z.writestr("OPS/package.opf", opf, ZIP_DEFLATED)
        z.writestr("OPS/text/chapter1.xhtml", CH1, ZIP_DEFLATED)
        if include_c2: z.writestr("OPS/chapters/chapter2.xhtml", CH2, ZIP_DEFLATED)
        if traversal: z.writestr("../escape.txt", "blocked", ZIP_DEFLATED)

def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    write_epub(ROOT / "valid-portuguese.epub")
    write_epub(ROOT / "missing-container.epub", container=None)
    write_epub(ROOT / "malformed-opf.epub", opf="<package><metadata>")
    write_epub(ROOT / "empty-spine.epub", opf=OPF.replace('<spine><itemref idref="c1"/><itemref idref="c2"/></spine>', '<spine/>'))
    write_epub(ROOT / "missing-spine-item.epub", opf=OPF.replace('idref="c2"', 'idref="missing"'))
    write_epub(ROOT / "traversal.epub", traversal=True)
    valid = (ROOT / "valid-portuguese.epub").read_bytes()
    (ROOT / "truncated.epub").write_bytes(valid[: len(valid) // 2])
    print(f"Fixtures generated in {ROOT}")

if __name__ == "__main__": main()

from pathlib import Path
import re
src=Path(__file__).parent
out=Path(__file__).parent.parent/'src'/'content.c'
files=[('maths','Cours_detaille_maths_physique_seconde.txt'),('pc1','Chapitre1_corps_purs_melanges.txt'),('pc2','Chapitre2_solutions_dissolution_dilution.txt')]
def ascii_clean(s):
    repl=str.maketrans({'é':'e','è':'e','ê':'e','ë':'e','à':'a','â':'a','î':'i','ï':'i','ô':'o','ö':'o','ù':'u','û':'u','ü':'u','ç':'c','É':'E','È':'E','À':'A','Ç':'C','œ':'oe','’':"'",'–':'-','—':'-'})
    return s.translate(repl).encode('ascii','ignore').decode().replace('\\','/').replace('"','\\"')
def read(name):
    return re.sub(r'\s+',' ',Path(src/name).read_text(encoding='utf-8')).strip()
def sections(kind, raw):
    lines=[re.sub(r'\s+',' ',x).strip() for x in raw.splitlines() if x.strip()]
    pats = (r'^(?:PARTIE|Chapitre|CHAPITRE|[0-9]+[.)-])',) if kind=='maths' else (r'^(?:[A-Z][A-Za-zÀ-ÿ -]{3,}|[0-9]+[.)-])',)
    res=[]
    for x in lines:
        y=ascii_clean(x)
        if kind=='maths' and (re.match(r'^(?:PARTIE|Chapitre|CHAPITRE|[0-9]+[.)-])',y) or y in ['Objectifs','Definitions - reels et intervalles','Formules cles','Methode generale']): res.append(y)
        elif kind!='maths' and len(y)>=5 and len(y)<100 and not y.startswith(('-', 'Cours :','TP','En autonomie','Appliquer','Sentrainer')) and not re.match(r'^\d+\.',y): res.append(y)
    # unique, first 18 meaningful headings
    seen=[]
    for x in res:
        if x not in seen: seen.append(x)
    return seen[:18]
chunks=[]
for kind,name in files:
    raw=Path(src/name).read_text(encoding='utf-8')
    text=ascii_clean(read(name))
    heads=sections(kind,raw)
    chunks.append((kind,text,heads))
with out.open('w',encoding='utf-8') as f:
    f.write('/* Generated from the supplied course TXT files. ASCII only. */\n#include <stddef.h>\n')
    for kind,text,heads in chunks:
        f.write(f'const char course_{kind}[] = "{text}";\n')
        f.write(f'const char * const course_{kind}_sections[] = {{\n')
        for h in heads: f.write(f'  "{h}",\n')
        f.write('};\n')
        f.write(f'const unsigned course_{kind}_section_count = {len(heads)};\n\n')

"""Read-only PHASMORA release and compatibility audit."""
from pathlib import Path
import hashlib,json,re,zipfile
root=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
bundle=root/'build/GHOST_artefacts/Release/VST3/PHASMORA.vst3'
metadata=json.loads(re.sub(r',\s*([}\]])',r'\1',(bundle/'Contents/Resources/moduleinfo.json').read_text()))
assert metadata['Name']=='PHASMORA' and metadata['Version']=='1.0.0'
assert metadata['Factory Info']['Vendor']=='PHASMORA'
assert [c['CID'] for c in metadata['Classes']]==['ABCDEF019182FAEB4768737447683031','ABCDEF011234ABCD4768737447683031']
assert all(c['Name']=='PHASMORA' and c['Version']=='1.0.0' and c['Vendor']=='PHASMORA' for c in metadata['Classes'])
assert metadata['Classes'][0]['Sub Categories']==['Fx']
expected={'Contents/Resources/moduleinfo.json','Contents/x86_64-win/PHASMORA.vst3'}
assert {p.relative_to(bundle).as_posix() for p in bundle.rglob('*') if p.is_file()}==expected
binary=(bundle/'Contents/x86_64-win/PHASMORA.vst3').read_bytes()
assert binary[:2]==b'MZ' and b'Users\\Admin' not in binary and b'Users/Admin' not in binary
baseline=json.loads((root/'Docs/PHASMORA-RenameBaseline.json').read_text())
for name,digest in baseline.items():
    p=root/name
    if p.name=='PluginProcessor.cpp':
        restored=p.read_text().replace('addPercent("ghost", "PHASMORA",','addPercent("ghost", "Ghost",')
    elif p.name=='PluginEditor.cpp':
        restored=p.read_text().replace('"ghost", "PHASMORA",','"ghost", "GHOST",').replace('GhostAssets::PhasmoraLogo_png','GhostAssets::GhostLogo_png')
    elif p.name=='GhostLookAndFeel.cpp':
        restored=p.read_text().replace('g.drawText("PHASMORA",','g.drawText("GHOST",').replace('"DRY / WET"','"DRY / GHOST"')
    else:
        assert sha(p)==digest,f'Protected source changed: {name}'
        continue
    # read_text normalizes Windows line endings; allow only that normalization
    # after reversing the explicit branding substitutions above.
    assert digest in {hashlib.sha256(text.encode()).hexdigest() for text in (restored, restored.replace('\n','\r\n'))},f'Unexpected non-branding source difference: {name}'
processor=(root/'Source/PluginProcessor.cpp').read_text()
assert re.findall(r'addPercent\("(\w+)"',processor)==['ghost','memory','decay','blur','drift','mix']
assert 'ParameterID { "reverse", 1 }' in processor and 'ParameterID { "freeze", 1 }' in processor
assert '"GHOST_STATE"' in processor
package=root/'dist/PHASMORA-1.0.0-Windows'
assert {p.name for p in package.iterdir()}=={'PHASMORA.vst3','ThirdPartyLicenses','PHASMORA-1.0.0-Windows-Setup.exe','README.md','LICENSE-PENDING.txt','THIRD-PARTY-NOTICES.txt','SHA256SUMS.txt'}
for relative in expected:assert (package/'PHASMORA.vst3'/relative).read_bytes()==(bundle/relative).read_bytes()
for name in ['README.md','LICENSE-PENDING.txt','THIRD-PARTY-NOTICES.txt']:assert (package/name).read_bytes()==(root/name).read_bytes()
with zipfile.ZipFile(root/'dist/PHASMORA-1.0.0-Windows.zip') as archive:
    for item in archive.infolist():
        name=item.filename.replace('\\','/')
        assert name.startswith('PHASMORA-1.0.0-Windows/')
        assert not any(part.lower() in ['build','source','tests','.tools','.codex'] for part in name.split('/'))
        assert not name.endswith(('.obj','.pdb','.log','.png'))
        if not item.is_dir():assert archive.read(item)==(root/'dist'/name).read_bytes()
for directory in [root/'dist',package]:
    for line in (directory/'SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert sha(directory/name)==digest
print('PASS: PHASMORA branding/version; legacy CIDs/IDs/state root; all DSP/preset/visualizer sources unchanged; only intended UI/metadata text changes; complete bundle, ZIP and SHA-256.')

#!/usr/bin/env python3
#
#  generate_project.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/12.
#  Copyright © 2026 laobamac. All rights reserved.
#
from pathlib import Path
import hashlib
import json
import plistlib

root = Path(__file__).resolve().parents[1]
source_info = plistlib.loads((root / 'Info.plist').read_bytes())
version = source_info['CFBundleVersion']
lilu_version = source_info['OSBundleLibraries']['as.vit9696.Lilu']
def uid(s): return hashlib.sha1(s.encode()).hexdigest()[:24].upper()
def q(s): return json.dumps(str(s))
objects = []
def obj(name, content):
    i = uid(name)
    objects.append(f'{i} = {{ {content} }};')
    return i

sources = sorted(str(p.relative_to(root)) for p in (root / 'Sources').glob('*.cpp'))
sources += ['Vendor/Lilu/Lilu/Library/plugin_start.cpp', 'Generated/Payload.cpp', 'Generated/Payload.S']
kxld = root / 'Vendor/xnu/libkern/kxld'
kxld_sources = sorted('Generated/kxld/' + p.name for p in kxld.glob('*.c') if not p.name.startswith('WKdm'))
if not kxld_sources:
    raise SystemExit('Run zsh Tools/prepare.zsh before generating the Xcode project.')
sources += kxld_sources
outputs = [p for p in sources if p.startswith(('Vendor/', 'Generated/'))]
outputs += sorted('Generated/kxld/' + p.name for p in kxld.glob('*.h'))
outputs += ['Generated/kxld/' + name for name in ('kxld.h', 'kxld_types.h', 'prelink.h')]
outputs += ['Generated/Headers/libkern/' + name for name in ('kxld.h', 'kxld_types.h', 'prelink.h')]
outputs += ['Generated/Headers/mach-o/stab.h', 'Generated/Headers/mach-o/x86_64/reloc.h']
files, builds = [], []
for p in sources + sorted(str(p.relative_to(root)) for p in (root / 'Sources').glob('*.h*')) + ['README.md', 'Info.plist']:
    ext = Path(p).suffix
    typ = {'.cpp': 'sourcecode.cpp.cpp', '.c': 'sourcecode.c.c', '.S': 'sourcecode.asm', '.hpp': 'sourcecode.cpp.h', '.h': 'sourcecode.c.h', '.plist': 'text.plist.xml'}.get(ext, 'text')
    f = obj('file:' + p, f'isa = PBXFileReference; lastKnownFileType = {q(typ)}; path = {q(p)}; sourceTree = "<group>";')
    files.append(f)
    if p in sources: builds.append(obj('build:' + p, f'isa = PBXBuildFile; fileRef = {f};'))
product = obj('product', 'isa = PBXFileReference; explicitFileType = "wrapper.kext"; path = SimpleHDA.kext; sourceTree = BUILT_PRODUCTS_DIR;')
products = obj('products', f'isa = PBXGroup; children = ({product},); name = Products; sourceTree = "<group>";')
group = obj('group', 'isa = PBXGroup; children = (' + ','.join(files + [products]) + ',); sourceTree = "<group>";')
prepare = obj('prepare', 'isa = PBXShellScriptBuildPhase; name = "Prepare Dependencies and Payload"; buildActionMask = 2147483647; files = (); inputPaths = (); outputPaths = (' + ','.join(q('$(SRCROOT)/' + path) for path in outputs) + ',); alwaysOutOfDate = 1; shellPath = /bin/zsh; shellScript = ' + q('set -e\n/bin/zsh "$SRCROOT/Tools/prepare.zsh"\n') + ';')
compilephase = obj('sources', f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(builds)},); runOnlyForDeploymentPostprocessing = 0;')
frameworkphase = obj('frameworks', 'isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0;')
settings = {
'ALWAYS_SEARCH_USER_PATHS':'NO', 'ARCHS':'x86_64', 'ONLY_ACTIVE_ARCH':'NO', 'SDKROOT':'macosx', 'SUPPORTED_PLATFORMS':'macosx', 'MACOSX_DEPLOYMENT_TARGET':'10.15',
'PRODUCT_NAME':'SimpleHDA', 'PRODUCT_BUNDLE_IDENTIFIER':'cn.laobamac.SimpleHDA', 'MODULE_NAME':'cn.laobamac.SimpleHDA',
'MODULE_VERSION':version, 'CURRENT_PROJECT_VERSION':version, 'MODULE_START':'SimpleHDA_kern_start', 'MODULE_STOP':'SimpleHDA_kern_stop',
'INFOPLIST_FILE':'Info.plist', 'CODE_SIGNING_ALLOWED':'NO', 'CODE_SIGN_IDENTITY':'', 'CLANG_CXX_LANGUAGE_STANDARD':'c++17',
'GCC_ENABLE_CPP_EXCEPTIONS':'NO', 'GCC_ENABLE_CPP_RTTI':'NO', 'GCC_ENABLE_KERNEL_DEVELOPMENT':'YES', 'GCC_SYMBOLS_PRIVATE_EXTERN':'YES',
'KERNEL_EXTENSION_HEADER_SEARCH_PATHS':'$(SRCROOT)/Vendor/MacKernelSDK/Headers', 'KERNEL_FRAMEWORK_HEADERS':'$(SRCROOT)/Vendor/MacKernelSDK/Headers',
'HEADER_SEARCH_PATHS':['$(SRCROOT)/Generated/Headers','$(SRCROOT)/Generated/kxld','$(SRCROOT)/Sources','$(SRCROOT)/Vendor/Lilu/Lilu'],
'LIBRARY_SEARCH_PATHS':['$(SRCROOT)/Vendor/MacKernelSDK/Library/universal'],
'GCC_PREPROCESSOR_DEFINITIONS':['KERNEL=1','KERNEL_PRIVATE=1','__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=1','PRODUCT_NAME=SimpleHDA','MODULE_VERSION='+version,'LILU_CUSTOM_KMOD_INIT=1'],
'OTHER_CFLAGS':['-fno-builtin','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-sse','-mno-sse2','-mno-mmx','-nostdinc','-Wno-deprecated-declarations','-Wno-sign-conversion','-Wno-shadow'],
'OTHER_LDFLAGS':['-nostdlib','-Wl,-no_fixup_chains','-Wl,-undefined,dynamic_lookup'],
'ENABLE_USER_SCRIPT_SANDBOXING':'NO', 'CLANG_ENABLE_MODULES':'NO', 'COPY_PHASE_STRIP':'NO', 'STRIP_INSTALLED_PRODUCT':'NO',
'CONFIGURATION_BUILD_DIR':'$(SRCROOT)/Build/$(CONFIGURATION)', 'DEBUG_INFORMATION_FORMAT':'dwarf-with-dsym',
'CLANG_WARN_DOCUMENTATION_COMMENTS':'NO', 'GCC_WARN_UNUSED_VARIABLE':'YES', 'GCC_WARN_UNUSED_FUNCTION':'YES', 'GCC_C_LANGUAGE_STANDARD':'gnu11',
}
configs = []
for name in ['Debug','Release']:
    s = dict(settings)
    s['GCC_OPTIMIZATION_LEVEL'] = '0' if name == 'Debug' else 's'
    if name == 'Debug': s['GCC_PREPROCESSOR_DEFINITIONS'] = s['GCC_PREPROCESSOR_DEFINITIONS'] + ['DEBUG=1']
    text = ''
    for k,v in s.items():
        value = '(' + ','.join(q(a) for a in v) + ',)' if isinstance(v, list) else q(v)
        text += f'{k} = {value};'
    configs.append(obj('config:' + name, f'isa = XCBuildConfiguration; buildSettings = {{{text}}}; name = {name};'))
configlist = obj('configlist', f'isa = XCConfigurationList; buildConfigurations = ({",".join(configs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
emptyconfigs = [obj('projectconfig:' + n, f'isa = XCBuildConfiguration; buildSettings = {{}}; name = {n};') for n in ['Debug','Release']]
projectlist = obj('projectlist', f'isa = XCConfigurationList; buildConfigurations = ({",".join(emptyconfigs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
target = obj('target', f'isa = PBXNativeTarget; buildConfigurationList = {configlist}; buildPhases = ({prepare},{compilephase},{frameworkphase},); buildRules = (); dependencies = (); name = SimpleHDA; productName = SimpleHDA; productReference = {product}; productType = "com.apple.product-type.kernel-extension";')
project = obj('project', f'isa = PBXProject; attributes = {{LastUpgradeCheck = 2650;}}; buildConfigurationList = {projectlist}; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; knownRegions = (en,Base,); mainGroup = {group}; productRefGroup = {products}; projectDirPath = ""; projectRoot = ""; targets = ({target},);')
p = root / 'SimpleHDA.xcodeproj'
(p / 'project.pbxproj').write_text('// !$*UTF8*$!\n//\n//  project.pbxproj\n//  SimpleHDA\n//\n//  Created by laobamac on 2026/8/18.\n//  Copyright © 2026 laobamac. All rights reserved.\n//\n{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {\n' + '\n'.join(objects) + f'\n}}; rootObject = {project}; }}\n')
(p / 'project.xcworkspace/contents.xcworkspacedata').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<Workspace version="1.0"><FileRef location="self:"></FileRef></Workspace>\n')
scheme = p / 'xcshareddata/xcschemes/SimpleHDA.xcscheme'
if not scheme.exists():
    scheme.write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="2650" version="1.3"><BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="SimpleHDA.kext" BlueprintName="SimpleHDA" ReferencedContainer="container:SimpleHDA.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction><LaunchAction buildConfiguration="Debug"/><ProfileAction buildConfiguration="Release"/><AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/></Scheme>
''')
info = {'CFBundleDevelopmentRegion':'en','CFBundleExecutable':'$(EXECUTABLE_NAME)','CFBundleIdentifier':'$(PRODUCT_BUNDLE_IDENTIFIER)','CFBundleInfoDictionaryVersion':'6.0','CFBundleName':'SimpleHDA','CFBundlePackageType':'KEXT','CFBundleShortVersionString':version,'CFBundleVersion':version,'OSBundleRequired':'Root', 'NSHumanReadableCopyright':'Copyright © 2026 laobamac. All rights reserved.', 'OSBundleLibraries': {'as.vit9696.Lilu':lilu_version, 'com.apple.kpi.bsd':'10.0.0','com.apple.kpi.iokit':'10.0.0','com.apple.kpi.libkern':'10.0.0','com.apple.kpi.mach':'10.0.0','com.apple.kpi.unsupported':'10.0.0'}, 'IOKitPersonalities': {'SimpleHDA': {'CFBundleIdentifier':'$(PRODUCT_BUNDLE_IDENTIFIER)','IOClass':'SimpleHDA','IOMatchCategory':'SimpleHDA','IOProviderClass':'IOResources','IOResourceMatch':'IOKit'}}}
(root / 'Info.plist').write_bytes(plistlib.dumps(info))

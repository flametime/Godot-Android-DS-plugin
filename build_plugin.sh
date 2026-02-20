#!/bin/bash
set -e

JOBS=8
ANDROID_API=24
VERSION="4.5"
GODOT_URL_VERSION="4.5.stable"
GODOT_TAG="4.5-stable"

echo -e "\033[0;34m=== AynThor Plugin: Automated Docker Build (Godot $VERSION) ===\033[0m"

WORK_DIR="/build/.build_cache"
DIST_DIR="/build/dist/AynThor"
SCONS_CACHE="/build/.scons_cache"

mkdir -p $WORK_DIR/cpp/src
mkdir -p $SCONS_CACHE
mkdir -p $DIST_DIR/bin/debug
mkdir -p $DIST_DIR/bin/release

echo -e "\033[0;32m--- Fast C++ Build (Only Plugin Source) ---\033[0m"
cp -r /build/CPP/* $WORK_DIR/cpp/src/
rm -f $WORK_DIR/cpp/src/*.os $WORK_DIR/cpp/src/*.obj

cd $WORK_DIR/cpp

cat <<EOF > SConstruct
import os
import sys

env = SConscript("/opt/godot-cpp/SConstruct")

env.Append(CPPPATH=["src"])

if env["platform"] == "android":
    env.Append(LIBS=["android", "vulkan", "log"])

if env["platform"] == "windows":
    env.Append(LIBS=["user32", "gdi32"])

CacheDir('$SCONS_CACHE')

sources = Glob("src/*.cpp")
library = env.SharedLibrary("bin/libaynthor_native", sources)
Default(library)
EOF

echo -e "\033[0;32m--- Running SCons (Android) ---\033[0m"
scons platform=android arch=arm64 target=template_release android_api_level=$ANDROID_API -j$JOBS

echo -e "\033[0;32m--- Running SCons (Windows) ---\033[0m"
scons platform=windows arch=x86_64 target=template_release use_mingw=yes -j$JOBS

cp bin/libaynthor_native.android.template_release.arm64.so $DIST_DIR/bin/libaynthor_native.so || cp bin/libaynthor_native.so $DIST_DIR/bin/libaynthor_native.so
cp bin/libaynthor_native.windows.template_release.x86_64.dll $DIST_DIR/bin/libaynthor_native.dll || cp bin/libaynthor_native.dll $DIST_DIR/bin/libaynthor_native.dll

echo -e "\033[0;32m--- Preparing Kotlin ---\033[0m"
KOTLIN_DIR="$WORK_DIR/kotlin"
PACKAGE_PATH="org/godot/plugins/aynthor"
mkdir -p "$KOTLIN_DIR/src/main/java/$PACKAGE_PATH"
cp /build/Kotlin/AynThorPlugin.kt "$KOTLIN_DIR/src/main/java/$PACKAGE_PATH/"

mkdir -p $KOTLIN_DIR/src/main
cat <<EOF > $KOTLIN_DIR/src/main/AndroidManifest.xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="org.godot.plugins.aynthor">
    <application>
        <meta-data
            android:name="org.godotengine.plugin.v2.AynThor"
            android:value="org.godot.plugins.aynthor.AynThorPlugin" />
    </application>
</manifest>
EOF

cd $KOTLIN_DIR
rm -rf build .gradle
rm -rf /root/.gradle/caches
if [ ! -f "godot-lib.aar" ]; then
    echo -e "\033[0;32m--- Downloading Godot Library ($GODOT_TAG) ---\033[0m"
    wget -q "https://github.com/godotengine/godot/releases/download/$GODOT_TAG/godot-lib.$GODOT_URL_VERSION.template_release.aar" -O godot-lib.aar
fi

cat <<EOF > settings.gradle
pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }
dependencyResolutionManagement { repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS); repositories { google(); mavenCentral() } }
rootProject.name = 'AynThor'
EOF

cat <<EOF > build.gradle
plugins { id 'com.android.library' version '8.2.0'; id 'org.jetbrains.kotlin.android' version '2.1.0' }
android { 
    namespace 'org.godot.plugins.aynthor'
    compileSdk 34
    defaultConfig { minSdk = 21 }
    compileOptions {
        sourceCompatibility JavaVersion.VERSION_17
        targetCompatibility JavaVersion.VERSION_17
    }
}
kotlin { jvmToolchain(17) }
dependencies { compileOnly files('godot-lib.aar') }
EOF

echo -e "\033[0;32m--- Building Android AAR ---\033[0m"
gradle assemble --no-daemon -Dorg.gradle.jvmargs="-Xmx2g" -Dorg.gradle.caching=true

find build/outputs/aar/ -name "*.aar" -exec cp {} $DIST_DIR/bin/release/AynThor-release.aar \;
cp $DIST_DIR/bin/release/AynThor-release.aar $DIST_DIR/bin/debug/AynThor-debug.aar

echo -e "\033[0;32m--- Done! ---\033[0m"
cp /build/GDScript/* $DIST_DIR/

echo -e "\033[0;34m=== SUCCESS! Plugin is ready in: AynThor/dist/AynThor ===\033[0m"

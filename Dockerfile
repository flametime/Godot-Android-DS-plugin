FROM eclipse-temurin:17-jdk

RUN apt-get update && apt-get install -y \
    python3 python3-pip scons git wget unzip curl cmake build-essential \
    mingw-w64 \
    && rm -rf /var/lib/apt/lists/*

ENV GRADLE_VERSION=8.5
RUN wget -q "https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip" -O /tmp/gradle.zip && \
    unzip -q /tmp/gradle.zip -d /opt && \
    ln -s /opt/gradle-${GRADLE_VERSION}/bin/gradle /usr/local/bin/gradle && \
    rm /tmp/gradle.zip

ENV ANDROID_SDK_ROOT /opt/android-sdk
RUN mkdir -p ${ANDROID_SDK_ROOT}/cmdline-tools && \
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O /tmp/tools.zip && \
    unzip -q /tmp/tools.zip -d ${ANDROID_SDK_ROOT}/cmdline-tools && \
    mv ${ANDROID_SDK_ROOT}/cmdline-tools/cmdline-tools ${ANDROID_SDK_ROOT}/cmdline-tools/latest && \
    rm /tmp/tools.zip

ENV PATH ${PATH}:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/platform-tools

RUN yes | sdkmanager --licenses && \
    sdkmanager "platforms;android-34" "build-tools;34.0.0" "ndk;28.1.13356709"

ENV ANDROID_NDK_HOME ${ANDROID_SDK_ROOT}/ndk/28.1.13356709

WORKDIR /opt/godot-cpp
RUN git clone --recursive https://github.com/godotengine/godot-cpp -b 4.5 .

RUN scons platform=android target=template_release arch=arm64 android_api_level=24 -j4
RUN scons platform=android target=template_debug arch=arm64 android_api_level=24 -j4
RUN scons platform=windows arch=x86_64 target=template_release use_mingw=yes -j4
RUN scons platform=windows arch=x86_64 target=template_debug use_mingw=yes -j4

WORKDIR /workspace
COPY build_plugin.sh /opt/build_plugin.sh
RUN chmod +x /opt/build_plugin.sh

ENTRYPOINT ["/opt/build_plugin.sh"]

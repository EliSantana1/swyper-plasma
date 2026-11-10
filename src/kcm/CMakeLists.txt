# SPDX-License-Identifier: GPL-3.0-or-later
# The System Settings page (Mouse & Touchpad > Touchpad Gestures)
kcmutils_add_qml_kcm(kcm_swyper SOURCES kcm.cpp)
target_compile_definitions(kcm_swyper PRIVATE TRANSLATION_DOMAIN="kcm_swyper")
target_link_libraries(kcm_swyper PRIVATE swypersettings KF6::KCMUtilsQuick KF6::I18n Qt6::DBus Qt6::Qml)

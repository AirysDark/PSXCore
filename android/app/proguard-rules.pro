# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in D:\Android\Sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.

# For Compose
-keepclassmembers class * extends androidx.compose.runtime.State { *; }
-keepclassmembers class * extends androidx.lifecycle.ViewModel { *; }

# For JSON parsing
-keep class com.airysdark.psxcore.model.** { *; }
-keep class org.json.** { *; }

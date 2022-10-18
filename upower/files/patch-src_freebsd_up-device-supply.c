--- src/freebsd/up-device-supply.c.orig	2022-07-21 09:06:49 UTC
+++ src/freebsd/up-device-supply.c
@@ -29,7 +29,7 @@
 #include <unistd.h>
 #include <sys/ioctl.h>
 #include <sys/types.h>
-#ifndef UPOWER_CI_DISABLE_PLATFORM_CODE
+#if !defined(UPOWER_CI_DISABLE_PLATFORM_CODE) && !defined(CWFG)
 #include <dev/acpica/acpiio.h>
 #endif
 
@@ -51,7 +51,7 @@ G_DEFINE_TYPE (UpDeviceSupply, up_device_supply, UP_TY
 
 G_DEFINE_TYPE (UpDeviceSupply, up_device_supply, UP_TYPE_DEVICE)
 
-static gboolean		 up_device_supply_refresh	 	(UpDevice *device, UpRefreshReason reason);
+static gboolean		up_device_supply_refresh	 	(UpDevice *device, UpRefreshReason reason);
 static gboolean		up_device_supply_acline_coldplug	(UpDevice *device);
 static gboolean		up_device_supply_battery_coldplug	(UpDevice *device, UpAcpiNative *native);
 static gboolean		up_device_supply_acline_set_properties	(UpDevice *device);
@@ -131,7 +131,7 @@ up_device_supply_battery_set_properties (UpDevice *dev
 static gboolean
 up_device_supply_battery_set_properties (UpDevice *device, UpAcpiNative *native)
 {
-#ifndef UPOWER_CI_DISABLE_PLATFORM_CODE
+#if !defined(UPOWER_CI_DISABLE_PLATFORM_CODE) && !defined(CWFG)
 	gint fd;
 	gdouble volt, dvolt, rate, lastfull, cap, dcap, lcap, capacity;
 	gboolean is_present;
@@ -250,7 +250,8 @@ up_device_supply_battery_set_properties (UpDevice *dev
 		      "energy", cap,
 		      "energy-full", lastfull,
 		      "energy-full-design", dcap,
-		      "energy-rate", rate,
+		      "energy-rate",
+              (battinfo.battinfo.state & ACPI_BATT_STAT_CHARGING) ? -rate : rate,
 		      "energy-empty", lcap,
 		      "voltage", volt,
 		      "capacity", capacity,
@@ -294,27 +295,60 @@ end:
 end:
 	close (fd);
 	return ret;
+#elif defined(CWFG)
+	int millivolt;
+	int charging;
+	int chargepct;
+	if(up_get_int_sysctl(&millivolt,NULL,"dev.cwfg.0.millivolt")) {
+		g_object_set(device,"voltage",millivolt/1000.0f,
+			     "is-present",1,NULL);
+	}else {
+		return FALSE;
+	}
+	if(up_get_int_sysctl(&charging,NULL,"dev.cwfg.0.charging")) {
+		g_object_set(device, "state",charging ? UP_DEVICE_STATE_CHARGING : UP_DEVICE_STATE_DISCHARGING,NULL);
+	       
+	} else {
+		return FALSE;
+	}
+	if(up_get_int_sysctl(&chargepct,NULL,"dev.cwfg.0.chargepct")) {
+		g_object_set(device, "percentage",(gdouble)chargepct,NULL);
+	       
+	} else {
+		return FALSE;
+	}
+
+	return TRUE;
+	
+	
+	
 #else
-	return FALSE;
+        return FALSE;
 #endif
-}
+}	
 
 /**
  * up_device_supply_acline_set_properties:
  **/
 static gboolean
 up_device_supply_acline_set_properties (UpDevice *device)
+
 {
-#ifndef UPOWER_CI_DISABLE_PLATFORM_CODE
+#if !defined(UPOWER_CI_DISABLE_PLATFORM_CODE) && !defined(CWFG) 
 	int acstate;
 
 	if (up_get_int_sysctl (&acstate, NULL, "hw.acpi.acline")) {
 		g_object_set (device, "online", acstate ? TRUE : FALSE, NULL);
 		return TRUE;
 	}
+	
+	return  FALSE;
+#elif defined(CWFG)
+	return FALSE;
+#else 
+	return FALSE;
 #endif
 
-	return FALSE;
 }
 
 /**
@@ -324,6 +358,7 @@ up_device_supply_coldplug (UpDevice *device)
 static gboolean
 up_device_supply_coldplug (UpDevice *device)
 {
+
 	UpAcpiNative *native;
 	const gchar *native_path;
 	const gchar *driver;
@@ -343,16 +378,16 @@ up_device_supply_coldplug (UpDevice *device)
 		ret = up_device_supply_acline_coldplug (device);
 		goto out;
 	}
-
-	if (!g_strcmp0 (driver, "battery")) {
+	if (!strcmp(driver, "battery")) {
 		ret = up_device_supply_battery_coldplug (device, native);
 		goto out;
 	}
 
 	g_warning ("invalid device %s with driver %s", native_path, driver);
-
+       
 out:
 	return ret;
+
 }
 
 /**

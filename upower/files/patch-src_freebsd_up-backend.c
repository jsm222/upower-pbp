--- src/freebsd/up-backend.c.orig	2022-10-18 03:05:34 UTC
+++ src/freebsd/up-backend.c
@@ -142,7 +142,8 @@ up_backend_acpi_devd_notify (UpBackend *backend, const
 
 	if (object != NULL)
 		g_object_unref (object);
-out:
+
+ out:
 	if (native != NULL)
 		g_object_unref (native);
 
@@ -219,7 +220,21 @@ up_backend_coldplug (UpBackend *backend, UpDaemon *dae
 
 	backend->priv->daemon = g_object_ref (daemon);
 	backend->priv->device_list = up_daemon_get_device_list (daemon);
+#ifdef CWFG
+	UpAcpiNative *native;
+			UpDevice *device;
+			GObject *object;
 
+	native = up_acpi_native_new_driver_unit ("battery", 0);
+			object = up_device_list_lookup (backend->priv->device_list, G_OBJECT (native));
+			if (object != NULL) {
+				device = UP_DEVICE (object);
+				g_warning ("treating add event as change event on %s", up_device_get_object_path (device));
+				up_device_refresh_internal (device, UP_REFRESH_EVENT);
+			} else {
+				up_backend_create_new_device (backend, native);
+			}
+#else
 	for (i = 0; i < (int) G_N_ELEMENTS (handlers); i++) {
 		int j;
 
@@ -246,11 +261,13 @@ up_backend_coldplug (UpBackend *backend, UpDaemon *dae
 			}
 		}
 	}
-
+#endif
 	up_backend_lid_coldplug (backend);
 
 	acnative = up_acpi_native_new ("hw.acpi.acline");
 	up_backend_create_new_device (backend, acnative);
+	
+  
 	g_object_unref (acnative);
 
 	up_devd_init (backend);

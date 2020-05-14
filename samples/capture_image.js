// Uses the media capture API to take an image and save it to the pictures library

var media = require('windows.media');
var capture = require('windows.media.capture');
var storage = require('windows.storage');
var mp = require('windows.media.mediaproperties');
var devices = require('windows.media.devices');

function captureImage(fileName) {
  storage.KnownFolders.picturesLibrary.createFileAsync(fileName, 
      storage.CreationCollisionOption.replaceExisting, function (err, newFile) {
    var storageFile = newFile;
    if (err) {
      return console.info('file', err);
    }
    cap.capturePhotoToStorageFileAsync(mp.ImageEncodingProperties.createJpeg(), storageFile, function (err, res) {
      if (err) {
        return console.info(err);
      }
      console.info('Image was succesfully saved to:', storageFile.path);
    });
  });
}

const cap = new capture.MediaCapture()

let settings = new capture.MediaCaptureInitializationSettings();
settings.streamingCaptureMode = capture.StreamingCaptureMode.video;
settings.sharingMode = capture.MediaCaptureSharingMode.sharedReadOnly;


cap.initializeAsync(settings, function (err, res) {
  if (err) {
    return console.info(err);
  }

  //const deviceController = cap.videoDeviceController

  // We can now use the VideoDeviceController regularly
  //deviceController.brightness.trySetValue(-1)
  //capture.videoDeviceController.brightness.trySetValue(-1);
  captureImage('image_capture_test.jpg');
});
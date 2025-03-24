const express = require('express');
const fs = require('fs');
const { exec } = require('child_process');
const app = express();

// Use express.raw() to handle raw binary data for image/jpeg
app.use('/upload', express.raw({ type: 'image/jpeg', limit: '10mb' }));

app.post('/upload', (req, res) => {
  // Save the captured image from the ESP32
  const imagePath = 'capture.jpg';
  fs.writeFile(imagePath, req.body, (err) => {
    if (err) {
      return res.status(500).send(err.toString());
    }
    console.log('Image saved. Running face recognition...');

    // Call the Python face recognition script with the saved image as argument
    exec(`I:/IoT_Lab/.venv/Scripts/python.exe face_recognition.py capture.jpg`, (error, stdout, stderr) => {
      if (error) {
        console.error(`Error: ${error}`);
        return res.status(500).send(error.toString());
      }
      console.log(`Face recognition output: ${stdout}`);
      res.send(stdout);
    });
    
  });
});

app.listen(8080, () => console.log("Server listening on port 8080"));

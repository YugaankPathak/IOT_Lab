#!/usr/bin/env python
import sys
import os
from deepface import DeepFace

# Folder containing known faces. Ensure this folder exists and contains images (e.g., "alice.jpg", "bob.jpg", etc.)
KNOWN_FOLDER = os.path.join(os.getcwd(), 'known')

# Get the test image file path from command-line argument
if len(sys.argv) < 2:
    print("No image provided.")
    sys.exit(1)

test_image = sys.argv[1]

recognized_name = None

# Iterate over each image in the known folder
for filename in os.listdir(KNOWN_FOLDER):
    known_image = os.path.join(KNOWN_FOLDER, filename)
    try:
        # DeepFace.verify returns a dictionary with a "verified" key.
        result = DeepFace.verify(img1_path=known_image, img2_path=test_image, enforce_detection=False)
        if result.get("verified"):
            # Assume the file name (without extension) is the person's name.
            recognized_name = os.path.splitext(filename)[0]
            break
    except Exception as e:
        print(f"Error comparing with {filename}: {e}")
        continue

if recognized_name:
    print(f"Face recognized as: {recognized_name}")
else:
    print("Face not recognized.")

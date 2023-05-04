# Spek
Code for the Spec calendar display platform. Built with an Arduino MRK WiFi 1010 and digitial display

Circuit is very simple. Connect VCC, Ground, SCL, and SCA to their respective pins on the board. Done!

Setting up the API is a rather tedious part of the process. Here are some steps:

Go to the Google Cloud Console.
Create a new project by clicking on the project dropdown menu at the top of the screen and selecting "New Project". Enter a name for your project and click on "Create".
Once your project is created, click on the hamburger menu on the left-hand side of the screen and select "APIs & Services" > "Dashboard".
Click on the "+ ENABLE APIS AND SERVICES" button and search for "Google Calendar API". Click on the "Google Calendar API" result and then click on "Enable".
Next, click on the "Credentials" tab on the left-hand side of the screen and then click on the "+ CREATE CREDENTIALS" button. Select "Service account" and enter a name for your service account.
In the "Role" field, select "Project" > "Editor". This will give your service account permission to manage all resources in your project.
Click on "Continue" and then "Done".
Once your service account is created, click on the three dots next to the service account and select "Manage keys".
Click on "Add Key" > "Create new key" and select "JSON" as the key type. This will download a private key file to your computer.
Rename the downloaded file to google-credentials.json and copy it to the root of your Arduino sketch directory.
Open the google-credentials.json file and copy the value of the client_email field.
Go to your Google Calendar, click on the gear icon on the top right and then click on "Settings".
Click on the "Calendars" tab and then click on the three dots next to the calendar you want to access with your Arduino.
Click on "Settings and sharing" and then scroll down to the "Integrate calendar" section.
Under "Access permissions", click on "Add people" and paste the value of the client_email field from step 11. Give the service account "Make changes to events" permission.

Create a Google Cloud Platform project and enable the Google Calendar API.
Create a service account and generate a private key.
Share your calendar with the service account and grant it "read" access.
Replace the placeholders in the code with your own values:
YOUR_SSID and YOUR_PASSWORD with your WiFi network credentials.
YOUR_API_KEY

Note that you will need to enable the Google Calendar API in your Google Cloud Console project before generating an API key. You can follow the steps I provided earlier to enable the Google Calendar API.

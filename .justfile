create PROJECT:
    cd ~/Downloads/git/sms/
    idf.py create-project {{PROJECT}}
    chmod -R u+rw {{PROJECT}}
    cd {{PROJECT}}
    idf.py set-target esp32s3

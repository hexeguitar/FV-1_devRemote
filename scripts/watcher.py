#!/usr/bin/env python3
"""
FV1 DevRemote file automatic file uploader
(c) 03.2021 by Piotr Zapart, www.hexefx.com

Required python packages:
1. pycurl
2. watchdog

Linux:
1. install packages required by pycurl:
    sudo apt install libcurl4-gnutls-dev librtmp-dev
2. install python packages:
    python3 -m pip install --user watchdog pycurl

Windows:
1. Check which version of python you have installed
2. Go to https://dl.bintray.com/pycurl/pycurl/
    and download compatible pycurl installer, ie.
    for python 3.8 the latest version at the moment is
    pycurl-7.43.0.5.win-amd64-py3.8.exe
3. Intall pycurl
4. Install watchdog:
    pip3 install watchdog
"""


import time
from pathlib import Path
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import pycurl
import os
import sys
import re
import argparse


def get_valid_filename(s):
    """
    Return the given string converted to a string that can be used for a clean
    filename. Remove leading and trailing spaces; convert other spaces to
    underscores; and remove anything that is not an alphanumeric,
    underscore, or dot.
    """
    s = str(s).strip().replace(' ', '_')
    return re.sub(r'(?u)[^\w.]', '', s)


def enable_file(file_name, fv1_url):
    url_addr = fv1_url + '/enable?file=/' + file_name
    c = pycurl.Curl()
    c.setopt(c.URL, url_addr)
    print(f'enabling file {file_name}')
    c.perform()
    url_addr = fv1_url + '/trigrefresh'
    c.setopt(c.URL, url_addr)
    c.perform()
    c.close()


def upload_file(file_path, fv1_url, verb):
    """
    Upload file using curl
    :param verb: verbose
    :param file_path:
    :param fv1_url:
    :return: abs file url path of uploaded file.
    """
    if file_path is None or not os.path.exists(file_path):
        print("File '{}' cant be uploaded".format(file_path))
        return
    valid_fname = get_valid_filename(Path(file_path).name)
    upload_url = fv1_url + '/uploadhex?f='
    c = pycurl.Curl()
    c.setopt(c.VERBOSE, int(verb))
    c.setopt(c.POST, 1)
    c.setopt(c.URL, upload_url)
    c.setopt(c.HTTPPOST, [("file1", (c.FORM_FILE, file_path, c.FORM_FILENAME, valid_fname))])
    print(f"Uploading file {file_path} as {valid_fname} to url {upload_url}")
    c.perform()
    c.close()
    return valid_fname


class Watcher:

    def __init__(self, pathToWatch, url, verb):
        self.observer = Observer()
        self.dir_to_watch = pathToWatch
        self.board_url = url
        self.ver_out = verb

    def run(self):
        event_handler = Handler(self.board_url, self.ver_out)
        self.observer.schedule(event_handler, self.dir_to_watch, recursive=True)
        self.observer.start()
        try:
            while True:
                time.sleep(5)
        except Exception:
            self.observer.stop()
            print("Error")

        self.observer.join()


class Handler(FileSystemEventHandler):
    board_url = []
    last_time = 0

    def __init__(self, url, verb):
        self.board_url = url
        self.verb_out = verb

    def on_modified(self, event):
        if event.is_directory:
            return None
        elif event.event_type == 'modified':
            # Taken any action here when a file is modified.
            # checks for the last modification time to avoid double triggers due to system operations
            # file extension is hex
            # length is 21517 (windows) or 20492 (unix/macos)
            file_stats = os.stat(event.src_path)
            new_time = int(file_stats.st_ctime)
            file_len = file_stats.st_size
            file_suffix = Path(event.src_path).suffix
            if new_time > self.last_time and file_suffix.upper() == '.HEX' and (file_len == 21517 or file_len == 20492):
                print('-'*32)
                print(f"File modified - {event.src_path}")
                valid_name = upload_file(event.src_path, self.board_url, self.verb_out)
                enable_file(valid_name, self.board_url)
                print('-' * 32)
                self.last_time = new_time


def __main(argv):
    parser = argparse.ArgumentParser(description="FV1 DevRemote auto file uploader. (c) 2021 by Piotr Zapart "
                                                 "www.hexefx.com",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-u', '--url', type=str, default='http://fv1.local', help="FV1 DevRemote base url")
    parser.add_argument('-d', '--dir', type=str, default=os.getcwd(), help="Directory to watch")
    parser.add_argument('-v', '--verbose', action='store_true', default=False, help="Verbose mode")
    args = parser.parse_args()
    print(f"URL of the board: {args.url}")
    print(f"Starting watcher in directory {args.dir}")
    w = Watcher(args.dir, args.url, args.verbose)
    w.run()


if __name__ == "__main__":
    __main(sys.argv[1:])

import os

def make_dir(dir_path):
    try:
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)
    except Exception as e:
        raise e

class DefaultConfig(object):
    PROJECT = "auto-scaler"

    # Get app root path, also can use flask.root_path.
    # ../../config.py
    PROJECT_ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
    INSTANCE_FOLDER_PATH = PROJECT_ROOT + "/instance"
    make_dir(INSTANCE_FOLDER_PATH)

    DEBUG = True
    TESTING = False
    LOGGING = True

    if LOGGING:
        LOG_FOLDER = os.path.join(INSTANCE_FOLDER_PATH, 'logs')
        make_dir(LOG_FOLDER)

    # SQLITE for prototyping.
    SQLALCHEMY_TRACK_MODIFICATIONS = False # Deprecated anyways
    SQLALCHEMY_DATABASE_URI = 'sqlite:///' + INSTANCE_FOLDER_PATH + '/db.sqlite'


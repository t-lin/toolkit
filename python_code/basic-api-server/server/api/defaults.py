from __future__ import absolute_import
from flask_restful import Resource, current_app, fields, marshal_with
from . import rest_api
import requests
import json

class tlintest(Resource):
    def get(self):
        return "You called GET!"

    def put(self):
        return "You called PUT!"


rest_api.add_resource(tlintest, '/tlintestapiurl')


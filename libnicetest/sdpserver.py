import json

from flask import Flask
app = Flask(__name__)

nextgameid = 0
games = {}

@app.route('/')
def hello_world():
    return 'Hello, World 2!'


@app.route('/get_games')
def get_games():
  return json.dumps(games)


@app.route('/create_game/<int:host_id>')
def create_game(host_id):
    games[nextgameid] = {host_id:{}}
    global nextgameid
    nextgameid += 1
    return "OK"

@app.route('/get_players/<int:game_id>')
def get_players(game_id):
    if game_id in games:
        return json.dumps(games[game_id])
    else:
        return "ERROR", 404

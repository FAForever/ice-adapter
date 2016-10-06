import json

from flask import Flask
app = Flask(__name__)

nextGameId = 0
games = {}

@app.route('/')
def get_games():
  return json.dumps(games)

@app.route('/create_game/<int:game_id>')
def create_game(game_id):
  if game_id in games:
    return "GAME ALREADY EXISTS", 409
  else:
    games[game_id] = {}
    return "OK"

@app.route('/join_game/<int:game_id>/<int:player_id>')
def join_game(game_id, player_id):
  if game_id in games:
    if player_id in games[game_id]:
      return "PLAYER ALREADY IN GAME", 409
    games[game_id][player_id] = {}
    return get_players(game_id)
  else:
    return "ERROR", 404

@app.route('/get_players/<int:game_id>')
def get_players(game_id):
  if game_id in games:
    return json.dumps(games[game_id])
  else:
    return "ERROR", 404

@app.route('/set_sdp/<int:game_id>/<int:player_id>/<int:remote_player_id>/<player_sdp>')
def set_sdp(game_id, player_id, remote_player_id, player_sdp):
  if not game_id in games:
    return "ERROR", 404
  if not player_id in games[game_id]:
    games[game_id][player_id] = {}
  games[game_id][player_id][remote_player_id] = player_sdp
  return "OK"

if __name__ == '__main__':
    app.run(debug=False)
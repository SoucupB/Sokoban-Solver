var height = 8;
var width = 8;

var sz = 55;
var images = {
  "wall": "unpassable.jpg",
  "terrain": "grass.jpg",
  "hole": "hole.jpg",
  "box": "box.jpg",
  "box_filled": "box_filled.png",
  "character": "character.jpg"
}

function createElementFromHTML(htmlString) {
  var div = document.createElement('div');
  div.innerHTML = htmlString.trim();
  return div.firstChild;
}

function img_Images(imgs) {
  let response = '';
  for(let i = 0; i < imgs.length; i++) {
    response += `
      <img class = 'cls-img' src = '${imgs[i]}' />
    `
  }
  return response
}

function box_Create(i, j, height, width, imgs) {
  return `
    <div style = "position: absolute; top: ${i * height}px; left: ${j * width}px; height: ${height}px; width: ${width}px;">
      ${img_Images(imgs)}
    </div>
  `;
}

function box_CreateAll(board) {
  let response = '';
  for(var i = 0; i < height; i++) {
    for(var j = 0; j < width; j++) {
      switch(board[i][j]) {
        case '#':
          response += box_Create(i, j, sz, sz, [images['wall']])
          break;
        case '.':
          response += box_Create(i, j, sz, sz, [images['hole']])
          break;
        case '$':
          response += box_Create(i, j, sz, sz, [images['box']])
          break;
        case '*':
          response += box_Create(i, j, sz, sz, [images['box_filled']])
          break;
        case '+':
          response += box_Create(i, j, sz, sz, [images['hole'], images['character']])
          break;
        case '@':
          response += box_Create(i, j, sz, sz, [images['terrain'], images['character']])
          break;
        case ' ':
          response += box_Create(i, j, sz, sz, [images['terrain']])
        default:
          response += box_Create(i, j, sz, sz, [])
          break;
      }
    }
  }
  return response
}

function grid_DrawMap(board) {
  return `
    <div style = 'position: relative;'>
      ${box_CreateAll(board)}
    </div>
  `;
}

function map_FromStr(stre) {
  let response = [];
  for(var i = 0; i < height; i++) {
    let row = [];
    for(var j = 0; j < width; j++) {
      row.push(" ");
    }
    response.push(row)
  }
  let clsResp = stre.split('\n');
  let currentRowIndex = 0;
  for(let i = 0; i < clsResp.length; i++) {
    if(clsResp[i].indexOf('#') != -1) {
      let row = [];
      for(let j = 0, c = Math.min(clsResp[i].length, width); j < c; j++) {
        row.push(clsResp[i][j]);
      }
      for(let i = 0, c = Math.min(clsResp[i].length, width); i < c; i++) {
        response[currentRowIndex][i] = row[i];
      }
      currentRowIndex++;
    }
  }
  return response;
}

function getBoardContent() {
  const elemDOM = document.getElementById('gm-input')
  if(elemDOM) {
    return elemDOM.value
  }
  return '';
}

function createGameMap(board) {
  let elementDOM = document.getElementById('game-map');
  if(elementDOM) {
    elementDOM.innerHTML = '';
    elementDOM.style.height = `${height * sz}px`;
    elementDOM.style.width = `${width * sz}px`;
    elementDOM.appendChild(createElementFromHTML(grid_DrawMap(board)))
  }
}

function _getPlayerPos(state) {
  for(let i = 0; i < height; i++) {
    for(let j = 0; j < width; j++) {
      if(state[i][j] == '@' || state[i][j] == '+') {
        return [i, j];
      }
    }
  }
  return null;
}

function _emptyLastState(coords, state) {
  switch(state[coords[0]][coords[1]]) {
    case '@':
      state[coords[0]][coords[1]] = ' ';
      break;
    case '+':
      state[coords[0]][coords[1]] = '.';
      break;
    default:
      break;
  }
}

function _fillNextBoxPosition(command, state, currentCoords) {
  if(!(currentCoords[0] >= 0 && currentCoords[0] < height && currentCoords[1] >= 0 && currentCoords[1] < width)) {
    return 0;
  }
  let nextCoords = _nextPosition(command, currentCoords);
  if(!nextCoords) {
    return 0;
  }
  switch(state[nextCoords[0]][nextCoords[1]]) {
    case ' ':
      state[nextCoords[0]][nextCoords[1]] = '$';
      return 1;
      break;
    case '.':
      state[nextCoords[0]][nextCoords[1]] = '*';
      return 1;
      break;
    default:
      break;
  }
  return 0;
}

function _fillNextPosition(coords, state, command) {
  if(!(coords[0] >= 0 && coords[0] < height && coords[1] >= 0 && coords[1] < width)) {
    return 0;
  }
  switch(state[coords[0]][coords[1]]) {
    case ' ':
      state[coords[0]][coords[1]] = '@';
      return 1;
      break;
    case '*':
      if(_fillNextBoxPosition(command, state, coords)) {
        state[coords[0]][coords[1]] = '+';
        return 1;
      }
      break;
    case '$':
      if(_fillNextBoxPosition(command, state, coords)) {
        state[coords[0]][coords[1]] = '@';
        return 1;
      }
      break;
    case '.':
      state[coords[0]][coords[1]] = '+';
      return 1;
      break;
    default:
      break;
  }
  return 0;
}

function _nextPosition(command, coords) {
  let deltaCoords = {
    'd': [1, 0],
    'u': [-1, 0],
    'l': [0, -1],
    'r': [0, 1]
  }
  let currentDelta = deltaCoords[command];
  if(currentDelta) {
    let newPos = [coords[0] + currentDelta[0], coords[1] + currentDelta[1]];
    if(newPos[0] >= 0 && newPos[0] < height && newPos[1] >= 0 && newPos[1] < width) {
      return newPos;
    }
  }
  return null;
}

function game_Move(command, state) {
  let initialPlayerPos = _getPlayerPos(state);
  if(!initialPlayerPos) {
    return ;
  }
  let nextPosition = _nextPosition(command, initialPlayerPos);
  if(nextPosition) {
    if(_fillNextPosition(nextPosition, state, command)) {
      _emptyLastState(initialPlayerPos, state);
    }
  }
}

function _clearIntervals() {
  const interval_id = window.setInterval(function(){}, Number.MAX_SAFE_INTEGER);
  for (let i = 1; i < interval_id; i++) {
    window.clearInterval(i);
  }
}

function game_DrawStatesByString(initialState, responseStr) {
  _clearIntervals();
  let initialPlayerPos = _getPlayerPos(initialState);
  let i = 0;
  responseStr = responseStr.toLowerCase();
  setInterval(function() {
    if(i >= responseStr.length) {
      return ;
    }
    let currentElement = responseStr[i++];
    game_Move(currentElement, initialState);
    createGameMap(initialState);
  }, 300);
}

function interpolation_FrameAt(frame, currentFrame, nextFrame) {

}

function setStringTo(str) {
  let domElem = document.getElementById('str-inp');
  if(domElem) {
    domElem.value = str;
  }
}

function onPress() {
  _clearIntervals();
  createGameMap(map_FromStr(getBoardContent()));
}

function _getInputString() {
  const elemDOM = document.getElementById('str-inp')
  if(elemDOM) {
    return elemDOM.value
  }
  return '';
}

function startGame() {
  createGameMap(map_FromStr(getBoardContent()));
  game_DrawStatesByString(map_FromStr(getBoardContent()), _getInputString());
}

function setCell(gameMap, y, x, val) {
  let flag = Module.ccall(
    'setMatrAt',  // name of C function
    'I8',  // return type
    ['I64', 'I8', 'I8', 'I8'],  // argument types
    [gameMap, y, x, val]  // arguments
  );
  return flag;
}

function registerGameMap(gameMap) {
  console.log(gameMap)
  let gameMapPointer = Module.ccall(
    'allocMatr',  // name of C function
    'I64',  // return type
    ['I8', 'I8'],  // argument types
    [height, width]  // arguments
  );
  for(let i = 0; i < height; i++) {
    for(let j = 0; j < width; j++) {
      if(!setCell(gameMapPointer, i, j, gameMap[i][j].charCodeAt(0))) {
        console.log(i, j, 'wroong')
        return null;
      }
    }
  }
  return gameMapPointer;
}

function show(gameMap) {
  Module.ccall(
    'showMapAt',  // name of C function
    'I64',  // return type
    ['I64'],  // argument types
    [gameMap]  // arguments
  );
}

function getSokoResponse(gameMap, it_y, it_x) {
  let gameMapPointer = Module.ccall(
    'getSokoResponse',  // name of C function
    'I64',  // return type
    ['I64', 'I8', 'I8'],  // argument types
    [gameMap, it_y, it_x]  // arguments
  );
  return gameMapPointer;
}

function getCharAtPos(buffer, position) {
  return Module.ccall(
    'getCharAtPos',  // name of C function
    'I64',  // return type
    ['I64', 'I32'],  // argument types
    [buffer, position]  // arguments
  );
}

function bufferToString(buffer) {
  let response = "";
  let index = 0;
  let currentChar = getCharAtPos(buffer, index);
  while(currentChar && index < 10001) {
    response += String.fromCharCode(currentChar)
    index++;
    currentChar = getCharAtPos(buffer, index);
  }
  return response;
}

function think() {
  let gameMap = map_FromStr(getBoardContent());
  let mapPointer = registerGameMap(gameMap);
  let it_x = 0;
  for(let i = 0; i < gameMap.length; i++) {
    console.log(gameMap[i])
    it_x = Math.max(it_x, gameMap[i].length);
  }
  let responseBuffer = getSokoResponse(mapPointer, gameMap.length, it_x);
  setStringTo(bufferToString(responseBuffer));
  startGame();
}

createGameMap(map_FromStr(getBoardContent()));



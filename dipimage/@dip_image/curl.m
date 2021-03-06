%CURL  Calculates the rotation of a vector image.
%   CURL(A,SIGMA) returns the rotation of a vector image, resulting
%   in a vector image with the same sizes.
%   SIGMA is the Gaussian smoothing. SIGMA defaults to 1.
%   A is expected to have as many dimensions as vector elements. The
%   operation is only defined for 2D or 3D vectors.
%
%   CURL(A,SIGMA,METHOD,BOUNDARY_CONDITION,PROCESS,TRUNCATION) defines
%   how the gradient is computed. See DERIVATIVE for a description of
%   these parameters and their defaults.
%
%   PROCESS determines along which dimensions to apply the operation.
%   If A has N dimensions but only M tensor elements, and PROCESS has
%   M unique elements, then the operation is correctly defined as
%   M-dimensional rotation.

% (c)2017, Cris Luengo.
% Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
% Based on original DIPimage code: (c)1999-2014, Delft University of Technology.
%
% Licensed under the Apache License, Version 2.0 (the "License");
% you may not use this file except in compliance with the License.
% You may obtain a copy of the License at
%
%    http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS,
% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
% See the License for the specific language governing permissions and
% limitations under the License.

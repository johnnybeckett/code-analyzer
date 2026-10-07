# Implementation Plan: Enhanced UML Viewer Layouts

## Overview
Implement enhanced layout engines for both CMake dependency visualization and class inheritance relationships to improve the clarity and understandability of complex code structures.

## Goals
1. Create a hierarchical layout engine for CMake views that minimizes dependency crossover
2. Add a similar hierarchical approach for class inheritance visualization
3. Maintain backward compatibility with existing layouts
4. Provide intuitive UI controls to switch between layout modes

## Implementation Details

### 1. CMake Layout Engine
- Replace circular layout with hierarchical approach using topological sorting
- Organize targets in layers based on dependency relationships
- Minimize edge crossings and visual clutter
- Maintain same API for compatibility

### 2. Class Inheritance Layout
- Add new "Hierarchical (inheritance)" layout option
- Use topological sorting to organize classes by inheritance relationships
- Base classes positioned above derived classes
- Integrate with existing UI controls

### 3. Technical Approach
- Implement topological sorting algorithms for dependency analysis
- Create layered positioning that reduces visual complexity
- Maintain performance with efficient graph traversal
- Ensure proper integration with existing three.js rendering system

## Files to Modify
- `src/uml/template.h` - Main HTML template with JavaScript layout implementations

## User Benefits
- Clearer visualization of CMake target dependencies
- Better understanding of class inheritance hierarchies  
- Reduced visual clutter in complex diagrams
- Intuitive switching between layout modes
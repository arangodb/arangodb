/**
 * Copyright (c) 2016, Lee Byron
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

declare module "iterall" {

  // Note: TypeScript already has built-in definitions for
  // Iterable<TValue> and Iterator<TValue> so they are not defined here.

  export var $$iterator: symbol | string

  export function isIterable(obj: any): boolean

  export function isArrayLike(obj: any): boolean

  export function isCollection(obj: any): boolean

  export function getIterator(iterable: Iterable<TValue>): Iterator<TValue>
  export function getIterator(iterable: any): void | Iterator<any>

  export function getIteratorMethod<TValue>(iterable: Iterable<TValue>): () => Iterator<TValue>
  export function getIteratorMethod(iterable: any): void | () => Iterator<any>

  export function forEach<TValue, TCollection extends Iterable<TValue>>(collection: TCollection, callbackFn: (value: TValue, index: number, collection: TCollection) => any, thisArg?: any): void
  export function forEach<TCollection extends { length: number }>(collection: TCollection, callbackFn: (value: any, index: number, collection: TCollection) => any, thisArg?: any): void

  export function createIterator<TValue>(collection: Iterable<TValue>): Iterator<TValue>
  export function createIterator(collection: { length: number }): Iterator<any>
  export function createIterator(collection: any): void | Iterator<any>

}
